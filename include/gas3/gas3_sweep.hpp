/**
 * gas3_sweep.hpp
 * --------------
 * Sweep Engine for Gas3 Module.
 *
 * Generates thermodynamic state datasets through:
 *   - Linear deterministic sweeps (Stage A)
 *   - Random uniform/logP sampling (Stage B)
 *   - Adaptive refinement near problem regions (Stage C)
 *
 * Every generated state is a GasStateRecord with full quality scoring.
 *
 * Anti-black-box: sweep parameters explicit, every point tagged with
 * provenance (sample_mode), every intermediate stored.
 */

#pragma once

#include "gas3_state_record.hpp"
#include "gas3_quality.hpp"
#include "gas2/gas2_engine.hpp"
#include "gas2/gas2_thermo.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <functional>

namespace vsepr {
namespace gas3 {

// ============================================================================
// Sweep configuration
// ============================================================================

struct SweepConfig {
    // Temperature range
    double T_min_K   = 100.0;
    double T_max_K   = 2000.0;
    double T_step_K  = 50.0;

    // Pressure range (atm)
    double P_min_atm = 0.5;
    double P_max_atm = 100.0;

    // Pressure grid points (explicit list for non-uniform spacing)
    std::vector<double> P_grid_atm = {0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0};

    // Species list (empty = all)
    std::vector<std::string> species_list;

    // EOS models to compare
    std::vector<gas2::EOSType> models = {
        gas2::EOSType::Ideal,
        gas2::EOSType::VanDerWaals,
        gas2::EOSType::RedlichKwong
    };

    double n_mol = 1.0;

    // Random sampling
    size_t random_count = 500;
    uint64_t seed = 42;

    // Adaptive refinement
    double adaptive_residual_threshold = 1e-4;
    int    adaptive_refine_depth = 2;

    // Min quality tier for fitting
    QualityTier fit_min_tier = QualityTier::Q2;
};

// ============================================================================
// Sweep statistics
// ============================================================================

struct SweepStats {
    size_t total_points     = 0;
    size_t converged        = 0;
    size_t failed           = 0;
    size_t Q0_count         = 0;
    size_t Q1_count         = 0;
    size_t Q2_count         = 0;
    size_t Q3_count         = 0;
    size_t Q4_count         = 0;
    double avg_residual     = 0.0;
    double max_residual     = 0.0;
    double avg_quality      = 0.0;
    double worst_quality    = 100.0;
    std::string worst_species;
    double worst_T = 0.0;
    double worst_P = 0.0;

    void update(const GasStateRecord& r) {
        total_points++;
        if (r.converged) converged++; else failed++;
        switch (r.quality_tier) {
            case QualityTier::Q0: Q0_count++; break;
            case QualityTier::Q1: Q1_count++; break;
            case QualityTier::Q2: Q2_count++; break;
            case QualityTier::Q3: Q3_count++; break;
            case QualityTier::Q4: Q4_count++; break;
        }
        if (!std::isnan(r.residual)) {
            avg_residual += std::abs(r.residual);
            if (std::abs(r.residual) > max_residual)
                max_residual = std::abs(r.residual);
        }
        avg_quality += r.quality_score;
        if (r.quality_score < worst_quality) {
            worst_quality = r.quality_score;
            worst_species = r.species;
            worst_T = r.T_K;
            worst_P = r.P_Pa / gas2::atm_to_Pa;
        }
    }

    void finalize() {
        if (total_points > 0) {
            avg_residual /= static_cast<double>(total_points);
            avg_quality  /= static_cast<double>(total_points);
        }
    }

    double convergence_rate() const {
        return total_points > 0 ?
            static_cast<double>(converged) / total_points * 100.0 : 0.0;
    }
};

// ============================================================================
// State generator — produces one GasStateRecord from Gas2
// ============================================================================

inline GasStateRecord generate_state(
    const std::string& formula,
    double T_K, double P_atm,
    gas2::EOSType eos_type,
    double n_mol,
    SampleMode mode,
    uint64_t run_id,
    uint64_t index)
{
    GasStateRecord r{};
    r.species      = formula;
    r.input_mode   = "PT";
    r.sample_mode  = mode;
    r.T_K          = T_K;
    r.P_Pa         = P_atm * gas2::atm_to_Pa;
    r.n_mol        = n_mol;
    r.run_id       = run_id;
    r.record_index = index;
    r.timestamp_utc = utc_timestamp();

    // Species lookup
    const auto* sp = gas2::find_species(formula);
    if (!sp) {
        r.model_name = "unknown";
        r.region = "unknown";
        r.converged = false;
        r.physically_valid = false;
        r.warning_flags = "SPECIES_NOT_FOUND";
        score_record(r);
        return r;
    }

    r.M_gmol = sp->molar_mass_g;
    double M_kg = sp->molar_mass_kg();
    double d_m = sp->d_kinetic_m();

    // Reduced state
    r.Tr = T_K / sp->Tc_K;
    r.Pr = P_atm / sp->Pc_atm;

    // Region classification
    if (r.Tr > 1.0 && r.Pr > 1.0)      r.region = "supercritical";
    else if (r.Tr > 1.2)                r.region = "gas";
    else if (r.Tr > 0.8 && r.Tr < 1.2)  r.region = "near-critical";
    else if (r.Tr < 0.8 && r.Pr < 0.5)  r.region = "gas";
    else                                 r.region = "questionable";

    // Solve EOS
    gas2::EOSResult eos;
    switch (eos_type) {
        case gas2::EOSType::Ideal:
            eos = gas2::ideal_gas(n_mol, T_K, r.P_Pa);
            r.model_name = "ideal";
            break;
        case gas2::EOSType::VanDerWaals:
            eos = gas2::vdw_solve_volume(n_mol, T_K, r.P_Pa, sp->vdw_a, sp->vdw_b);
            r.model_name = "vdW";
            break;
        case gas2::EOSType::RedlichKwong: {
            auto rk = gas2::rk_params_from_critical(sp->Tc_K, sp->Pc_atm * gas2::atm_to_Pa);
            eos = gas2::rk_solve_volume(n_mol, T_K, r.P_Pa, rk.a_RK, rk.b_RK);
            r.model_name = "RK";
            break;
        }
    }

    r.V_m3       = eos.V_m3;
    r.Z          = eos.Z;
    r.iterations = eos.iterations;
    r.converged  = eos.converged;

    // Compute residual: |P_calc - P_input| / P_input
    if (r.V_m3 > 0 && r.n_mol > 0) {
        double P_calc = r.n_mol * gas2::R_gas * T_K / r.V_m3;  // ideal back-check
        if (eos_type == gas2::EOSType::Ideal)
            r.residual = 0.0;
        else
            r.residual = std::abs(r.P_Pa - P_calc) / r.P_Pa;
    }

    // Density
    r.rho_kgm3 = (r.V_m3 > 0) ? (r.n_mol * M_kg / r.V_m3) : NAN;

    // Kinetic theory
    r.v_rms  = gas2::rms_speed(T_K, M_kg);
    r.v_mean = gas2::mean_speed(T_K, M_kg);
    r.v_mp   = gas2::most_probable_speed(T_K, M_kg);
    r.mfp_m  = gas2::mean_free_path(T_K, r.P_Pa, d_m);

    // Heat capacity
    int n_atoms = sp->n_atoms;
    bool linear = (n_atoms <= 2);
    auto dof = gas2::compute_dof(n_atoms, linear);
    r.Cv_JmolK = gas2::Cv_from_dof(dof.total_classical);
    r.Cp_JmolK = gas2::Cp_from_dof(dof.total_classical);
    r.gamma    = gas2::gamma_from_dof(dof.total_classical);
    r.c_sound  = gas2::speed_of_sound(T_K, M_kg, r.gamma);

    // Joule-Thomson
    r.mu_JT = gas2::joule_thomson_vdw(T_K, sp->vdw_a, sp->vdw_b, r.Cp_JmolK);

    // Thermodynamic potentials (ideal gas approximation)
    // Internal energy per mole: U = (f/2) * R * T
    r.u_Jmol = 0.5 * dof.total_classical * gas2::R_gas * T_K;
    // Enthalpy per mole: H = U + PV = U + RT (ideal) or U + ZRT
    r.h_Jmol = r.u_Jmol + r.Z * gas2::R_gas * T_K;
    // Entropy: Sackur-Tetrode (ideal, per mole)
    if (r.V_m3 > 0 && r.n_mol > 0) {
        auto entr = gas2::entropy_ideal(r.n_mol, T_K, r.V_m3, sp->molar_mass_g);
        r.s_JmolK = entr.S_per_mol;
    }

    // Fugacity
    if (eos_type == gas2::EOSType::VanDerWaals && r.V_m3 > sp->vdw_b * n_mol) {
        double Vm = r.V_m3 / r.n_mol;
        r.phi = gas2::fugacity_coeff_vdw(T_K, Vm, sp->vdw_a, sp->vdw_b);
        r.f_Pa = r.phi * r.P_Pa;
    } else {
        r.phi = 1.0;
        r.f_Pa = r.P_Pa;
    }

    // Physical validity check
    r.physically_valid = r.converged && r.V_m3 > 0 && r.Z > 0 &&
                         !std::isnan(r.Z) && !std::isinf(r.Z) &&
                         r.rho_kgm3 > 0;

    // Score
    score_record(r);
    return r;
}

// ============================================================================
// Linear sweep (Stage A)
// ============================================================================

inline std::vector<GasStateRecord> linear_sweep(
    const SweepConfig& cfg,
    SweepStats& stats,
    std::function<void(const GasStateRecord&)> on_record = nullptr)
{
    std::vector<GasStateRecord> records;
    uint64_t run_id = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    uint64_t idx = 0;

    // Determine species list
    std::vector<std::string> species;
    if (cfg.species_list.empty()) {
        for (const auto& [key, _] : gas2::species_database())
            species.push_back(key);
    } else {
        species = cfg.species_list;
    }

    for (const auto& formula : species) {
        for (double T = cfg.T_min_K; T <= cfg.T_max_K + 0.1; T += cfg.T_step_K) {
            for (double P_atm : cfg.P_grid_atm) {
                for (auto eos_type : cfg.models) {
                    auto r = generate_state(formula, T, P_atm, eos_type,
                                             cfg.n_mol, SampleMode::Linear,
                                             run_id, idx++);
                    stats.update(r);
                    if (on_record) on_record(r);
                    records.push_back(std::move(r));
                }
            }
        }
    }

    stats.finalize();
    return records;
}

// ============================================================================
// Random sweep (Stage B)
// ============================================================================

inline std::vector<GasStateRecord> random_sweep(
    const SweepConfig& cfg,
    SweepStats& stats,
    std::function<void(const GasStateRecord&)> on_record = nullptr)
{
    std::vector<GasStateRecord> records;
    uint64_t run_id = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    uint64_t idx = 0;

    std::mt19937_64 rng(cfg.seed);
    std::uniform_real_distribution<double> T_dist(cfg.T_min_K, cfg.T_max_K);
    std::uniform_real_distribution<double> P_dist(cfg.P_min_atm, cfg.P_max_atm);

    // Log-pressure distribution
    double log_P_min = std::log10(cfg.P_min_atm);
    double log_P_max = std::log10(cfg.P_max_atm);
    std::uniform_real_distribution<double> logP_dist(log_P_min, log_P_max);

    std::vector<std::string> species;
    if (cfg.species_list.empty()) {
        for (const auto& [key, _] : gas2::species_database())
            species.push_back(key);
    } else {
        species = cfg.species_list;
    }

    std::uniform_int_distribution<size_t> sp_dist(0, species.size() - 1);

    for (size_t i = 0; i < cfg.random_count; ++i) {
        const auto& formula = species[sp_dist(rng)];
        double T = T_dist(rng);

        // Alternate between uniform and logP
        SampleMode mode;
        double P_atm;
        if (i % 2 == 0) {
            P_atm = P_dist(rng);
            mode = SampleMode::Random_Uniform;
        } else {
            P_atm = std::pow(10.0, logP_dist(rng));
            mode = SampleMode::Random_LogP;
        }

        // Use VdW for random sweep (the interesting model)
        auto r = generate_state(formula, T, P_atm,
                                 gas2::EOSType::VanDerWaals,
                                 cfg.n_mol, mode, run_id, idx++);
        stats.update(r);
        if (on_record) on_record(r);
        records.push_back(std::move(r));
    }

    stats.finalize();
    return records;
}

// ============================================================================
// Adaptive refinement (Stage C)
// ============================================================================

inline std::vector<GasStateRecord> adaptive_sweep(
    const std::vector<GasStateRecord>& prior_records,
    const SweepConfig& cfg,
    SweepStats& stats,
    std::function<void(const GasStateRecord&)> on_record = nullptr)
{
    std::vector<GasStateRecord> records;
    uint64_t run_id = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    uint64_t idx = 0;

    // Find problem regions: Q0 or Q1 points
    for (const auto& r : prior_records) {
        if (r.quality_tier > QualityTier::Q1) continue;

        // Generate points around the problem region
        double T_center = r.T_K;
        double P_center = r.P_Pa / gas2::atm_to_Pa;

        for (int depth = 0; depth < cfg.adaptive_refine_depth; ++depth) {
            double T_delta = cfg.T_step_K / std::pow(2.0, depth + 1);
            double P_delta = P_center * 0.1 / std::pow(2.0, depth);

            double T_vals[] = {T_center - T_delta, T_center, T_center + T_delta};
            double P_vals[] = {P_center - P_delta, P_center, P_center + P_delta};

            for (double T : T_vals) {
                if (T < cfg.T_min_K || T > cfg.T_max_K) continue;
                for (double P : P_vals) {
                    if (P < cfg.P_min_atm || P > cfg.P_max_atm) continue;

                    auto rec = generate_state(r.species, T, P,
                                               gas2::EOSType::VanDerWaals,
                                               cfg.n_mol, SampleMode::Adaptive,
                                               run_id, idx++);
                    stats.update(rec);
                    if (on_record) on_record(rec);
                    records.push_back(std::move(rec));
                }
            }
        }
    }

    stats.finalize();
    return records;
}

// ============================================================================
// Filter records by quality tier
// ============================================================================

inline std::vector<GasStateRecord> filter_by_tier(
    const std::vector<GasStateRecord>& records,
    QualityTier min_tier)
{
    std::vector<GasStateRecord> out;
    for (const auto& r : records) {
        if (static_cast<int>(r.quality_tier) >= static_cast<int>(min_tier))
            out.push_back(r);
    }
    return out;
}

} // namespace gas3
} // namespace vsepr
