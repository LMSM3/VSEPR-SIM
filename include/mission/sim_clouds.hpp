#pragma once
/**
 * sim_clouds.hpp
 * ==============
 * V2 — Gas Clouds
 * Scale Mission: Particles, Clouds, Lattice, and Pipe Gas 3
 *
 * Statistical / semi-discrete gas ensemble simulation.
 * Represents gases as cloud-scale evolving populations.
 *
 * Physical scope:
 *   - EOS (ideal, VdW, Redlich-Kwong via gas2)
 *   - Compressibility
 *   - Kinetic estimates (Maxwell-Boltzmann)
 *   - Species fraction mixing  χ_i
 *   - Diffusion / spatial spread
 *   - Temperature-pressure response
 *   - Optional reaction readiness
 *
 * Core cloud state:
 *   C = {ρ, T, P, u, χ_1, χ_2, …, χ_n}
 *
 * Stress bands (from mission work order):
 *   Instant    : 1 cloud, static property solve
 *   Short_50ms : 1–3 clouds, one-step estimate
 *   Medium_5s  : 1–10 clouds, time evolution, mixing + pressure changes
 *   Long_5min  : many clouds, broad P-T sweeps, cloud-cloud interaction
 *
 * Deliverables:
 *   - cloud state tables
 *   - species fraction heatmaps (CSV)
 *   - expansion / compression tracks
 *   - gas3-compatible summary output
 *
 * Integrates with:
 *   include/mission/mission_profile.hpp  — shared profile + entity layer
 *   include/gas2/gas2_engine.hpp         — EOS + kinetic + thermal
 *   include/gas2/gas2_species.hpp        — GasSpecies database
 *   include/gas3/gas3_state_record.hpp   — QualityTier for output records
 */

#include "mission/mission_profile.hpp"
#include "gas2/gas2_engine.hpp"
#include "gas3/gas3_state_record.hpp"

#include <vector>
#include <string>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <optional>
#include <chrono>

namespace vsepr {
namespace mission {
namespace clouds {

// ============================================================================
// RuntimeProfile factory for V2 Gas Clouds
// ============================================================================

inline RuntimeProfile profile_for(MissionScale s) {
    RuntimeProfile p{};
    p.scale               = s;
    p.pairwise_interactions = false; // cloud-level, not per-molecule
    p.export_csv          = true;

    switch (s) {
        case MissionScale::Instant:
            p.max_entities       = 1;
            p.max_steps          = 0;       // single-point solve only
            p.dt                 = 0.0;
            p.high_accuracy      = false;
            p.neighbor_list      = false;
            p.export_full_history = false;
            p.export_markdown    = false;
            p.export_xyz         = false;
            p.approximation_level = 0;
            p.wall_budget_s      = 0.0;
            break;

        case MissionScale::Short_50ms:
            p.max_entities       = 3;
            p.max_steps          = 1;
            p.dt                 = 1.0;     // 1 s per cloud-step
            p.high_accuracy      = false;
            p.neighbor_list      = false;
            p.export_full_history = false;
            p.export_markdown    = false;
            p.export_xyz         = false;
            p.approximation_level = 0;
            p.wall_budget_s      = 0.05;
            break;

        case MissionScale::Medium_5s:
            p.max_entities       = 10;
            p.max_steps          = 50;
            p.dt                 = 1.0;
            p.high_accuracy      = true;
            p.neighbor_list      = false;
            p.export_full_history = false;
            p.export_markdown    = true;
            p.export_xyz         = false;
            p.approximation_level = 0;
            p.wall_budget_s      = 5.0;
            break;

        case MissionScale::Long_5min:
            p.max_entities       = 200;
            p.max_steps          = 500;
            p.dt                 = 1.0;
            p.high_accuracy      = true;
            p.neighbor_list      = true;    // cloud-cloud spatial lookup
            p.export_full_history = true;
            p.export_markdown    = true;
            p.export_xyz         = false;
            p.approximation_level = 0;
            p.wall_budget_s      = 300.0;
            break;
    }
    return p;
}

// ============================================================================
// Species fraction — one component of a mixture
// ============================================================================

struct SpeciesFraction {
    std::string formula;
    double      chi;          // mole fraction [0, 1]
    const gas2::GasSpecies* species {nullptr};  // resolved from gas2 DB, nullable
};

// ============================================================================
// Cloud state  C = {ρ, T, P, u, χ_1…χ_n}
// ============================================================================

struct CloudState {
    // Thermodynamic state
    double rho_kg_m3  {1.2};      // density (kg/m³)
    double T_K        {300.0};    // temperature (K)
    double P_Pa       {101325.0}; // pressure (Pa)
    double u_m_s      {0.0};      // bulk flow speed (m/s)

    // Mixture fractions  Σ χ_i = 1
    std::vector<SpeciesFraction> fractions;

    // Derived / cached
    double Z          {1.0};      // compressibility factor
    double molar_mass {28.97};    // mixture molar mass g/mol (air default)
    double Cp_mix     {29.1};     // J/(mol·K)
    double gamma_mix  {1.4};      // Cp/Cv mixture
    double sound_speed{343.0};    // m/s

    // Spatial extent (simple sphere model for cloud-cloud interaction)
    std::array<double,3> centre  {0.0, 0.0, 0.0}; // m
    double               radius  {1.0};             // m

    // Quality flag
    gas3::QualityTier quality {gas3::QualityTier::Q2};

    // History entry for time series
    struct Snapshot {
        double time_s;
        double T_K, P_Pa, rho, Z;
    };
    std::vector<Snapshot> history;

    void record_snapshot(double t) {
        history.push_back({t, T_K, P_Pa, rho_kg_m3, Z});
    }
};

// ============================================================================
// Cloud initialisation helpers
// ============================================================================

inline CloudState make_cloud(
    double T_K,
    double P_Pa,
    std::initializer_list<std::pair<std::string,double>> mix)
{
    CloudState c;
    c.T_K  = T_K;
    c.P_Pa = P_Pa;

    double sum_chi = 0.0;
    for (auto& [formula, chi] : mix) {
        SpeciesFraction sf;
        sf.formula = formula;
        sf.chi     = chi;
        sf.species = gas2::find_species(formula);
        c.fractions.push_back(sf);
        sum_chi += chi;
    }
    // Normalise fractions
    if (sum_chi > 1e-12)
        for (auto& sf : c.fractions) sf.chi /= sum_chi;

    // Compute mixture molar mass + Cp
    double M_mix = 0.0, Cp_mix = 0.0, Cv_mix = 0.0;
    for (auto& sf : c.fractions) {
        if (sf.species) {
            M_mix  += sf.chi * sf.species->molar_mass_g;
            Cp_mix += sf.chi * sf.species->Cp_Jmol;
            Cv_mix += sf.chi * sf.species->Cv_Jmol;
        }
    }
    c.molar_mass = (M_mix > 0.0) ? M_mix : 28.97;
    c.Cp_mix     = (Cp_mix > 0.0) ? Cp_mix : 29.1;
    c.gamma_mix  = (Cv_mix > 1e-6) ? Cp_mix / Cv_mix : 1.4;

    // Ideal gas density baseline  ρ = PM/(RT)
    const double R = 8.314462;
    c.rho_kg_m3 = P_Pa * (c.molar_mass * 1e-3) / (R * T_K);

    // Sound speed  c_s = sqrt(γ RT / M)
    c.sound_speed = std::sqrt(c.gamma_mix * R * T_K / (c.molar_mass * 1e-3));

    // VdW compressibility using dominant species
    c.Z = 1.0;
    if (!c.fractions.empty() && c.fractions[0].species) {
        const auto& sp0 = *c.fractions[0].species;
        auto res = gas2::vdw_solve_volume(1.0, T_K, P_Pa, sp0.vdw_a, sp0.vdw_b);
        if (res.converged) {
            c.Z = res.Z;
            c.quality = (std::abs(c.Z - 1.0) < 0.05)
                ? gas3::QualityTier::Q3 : gas3::QualityTier::Q2;
        }
    }

    return c;
}

// ============================================================================
// Single-step cloud evolution channel
//
// Implements adiabatic free expansion / compression:
//   T_new = T * (P_new / P)^((γ-1)/γ)
//   ρ_new = P_new M / (R T_new)
//   Z_new = recomputed from VdW
//
// Contract:
//   Required state: T_K, P_Pa, rho, gamma_mix, molar_mass
//   Writes:         T_K, P_Pa, rho, Z, sound_speed
//   Local only:     yes (no pairwise between clouds here)
//   dt_max:         large (thermodynamic relaxation timescale)
// ============================================================================

inline void evolve_cloud(CloudState& c, double P_new_Pa) {
    const double R    = 8.314462;
    const double gam  = c.gamma_mix;
    const double exp  = (gam - 1.0) / gam;

    c.T_K  = c.T_K * std::pow(P_new_Pa / c.P_Pa, exp);
    c.P_Pa = P_new_Pa;
    c.rho_kg_m3 = P_new_Pa * (c.molar_mass * 1e-3) / (R * c.T_K);
    c.sound_speed = std::sqrt(gam * R * c.T_K / (c.molar_mass * 1e-3));

    // Recompute Z for dominant species
    if (!c.fractions.empty() && c.fractions[0].species) {
        const auto& sp0 = *c.fractions[0].species;
        auto res = gas2::vdw_solve_volume(1.0, c.T_K, c.P_Pa, sp0.vdw_a, sp0.vdw_b);
        if (res.converged) c.Z = res.Z;
    }
}

// ============================================================================
// Cloud system (ensemble of clouds)
// ============================================================================

struct CloudSystem {
    std::vector<CloudState> clouds;
    RuntimeProfile          profile;
    std::size_t             step      {0};
    double                  sim_time  {0.0};
};

// ============================================================================
// Main run — deterministic cloud scheduler
// ============================================================================

inline MissionDeliverable run(CloudSystem& sys) {
    MissionDeliverable d{};
    d.sim_version  = "V2_Clouds";
    d.scale        = sys.profile.scale;
    d.entity_count = sys.clouds.size();

    auto t_start = std::chrono::steady_clock::now();

    if (sys.profile.max_steps == 0) {
        // Instant: single-point property evaluation only
        for (auto& c : sys.clouds) c.record_snapshot(0.0);
    } else {
        const double dt = sys.profile.dt;

        for (std::size_t step = 0; step < sys.profile.max_steps; ++step) {
            const double t = (step + 1) * dt;
            // Simple pressure relaxation: each cloud drifts 0.1% toward 1 atm
            for (auto& c : sys.clouds) {
                double P_target = 101325.0;
                double P_new = c.P_Pa + 0.001 * (P_target - c.P_Pa);
                evolve_cloud(c, P_new);
                if (sys.profile.export_full_history) c.record_snapshot(t);
            }
            sys.step     = step + 1;
            sys.sim_time = t;

            if (sys.profile.wall_budget_s > 0.0) {
                auto elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - t_start).count();
                if (elapsed > sys.profile.wall_budget_s) break;
            }
        }
        if (!sys.profile.export_full_history)
            for (auto& c : sys.clouds) c.record_snapshot(sys.sim_time);
    }

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    // Summary
    double T_mean = 0.0, P_mean = 0.0;
    for (const auto& c : sys.clouds) { T_mean += c.T_K; P_mean += c.P_Pa; }
    if (!sys.clouds.empty()) {
        T_mean /= sys.clouds.size();
        P_mean /= sys.clouds.size();
    }

    d.steps_run       = sys.step;
    d.wall_time_s     = elapsed;
    d.energy_total    = 0.0;
    d.converged       = true;
    d.temperature_out = T_mean;
    d.pressure_out    = P_mean;

    return d;
}

// ============================================================================
// CSV export — cloud state table
// ============================================================================

inline std::string to_csv(const CloudSystem& sys) {
    std::ostringstream o;
    o << "cloud_id,formula_0,chi_0,T_K,P_Pa,rho_kg_m3,Z,gamma,sound_speed_m_s,quality\n";
    for (std::size_t i = 0; i < sys.clouds.size(); ++i) {
        const auto& c = sys.clouds[i];
        std::string f0 = c.fractions.empty() ? "" : c.fractions[0].formula;
        double chi0 = c.fractions.empty() ? 0.0 : c.fractions[0].chi;
        o << i << "," << f0 << "," << chi0 << ","
          << std::fixed << std::setprecision(3)
          << c.T_K << "," << c.P_Pa << "," << c.rho_kg_m3 << ","
          << c.Z << "," << c.gamma_mix << "," << c.sound_speed << ","
          << gas3::tier_name(c.quality) << "\n";
    }
    return o.str();
}

// ============================================================================
// Console report
// ============================================================================

inline std::string report(const CloudSystem& sys, const MissionDeliverable& d) {
    std::ostringstream o;
    o << "\n  V2 Gas Clouds — " << mission_scale_name(sys.profile.scale) << "\n";
    o << "  " << std::string(60, '-') << "\n";
    o << "  Clouds    : " << d.entity_count << "\n";
    o << "  Steps     : " << d.steps_run << "\n";
    o << "  Wall time : " << std::fixed << std::setprecision(4) << d.wall_time_s << " s\n";
    if (d.temperature_out) o << "  T_mean    : " << *d.temperature_out << " K\n";
    if (d.pressure_out)    o << "  P_mean    : " << *d.pressure_out    << " Pa\n";
    o << "\n  Cloud states:\n";
    o << "  " << std::left << std::setw(4) << "#"
      << std::setw(10) << "Species"
      << std::setw(10) << "T (K)"
      << std::setw(12) << "P (Pa)"
      << std::setw(8) << "Z"
      << std::setw(10) << "γ"
      << "Quality\n";
    o << "  " << std::string(58, '-') << "\n";
    for (std::size_t i = 0; i < sys.clouds.size(); ++i) {
        const auto& c = sys.clouds[i];
        std::string sp = c.fractions.empty() ? "?" : c.fractions[0].formula;
        o << "  " << std::left << std::setw(4) << i
          << std::setw(10) << sp
          << std::setw(10) << std::fixed << std::setprecision(1) << c.T_K
          << std::setw(12) << std::fixed << std::setprecision(0) << c.P_Pa
          << std::setw(8)  << std::fixed << std::setprecision(4) << c.Z
          << std::setw(10) << std::fixed << std::setprecision(3) << c.gamma_mix
          << gas3::tier_name(c.quality) << "\n";
    }
    return o.str();
}

} // namespace clouds
} // namespace mission
} // namespace vsepr
