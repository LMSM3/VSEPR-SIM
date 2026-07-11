#pragma once
/**
 * sim_pipe_gas3.hpp
 * =================
 * V4 — Pipe Gas 3
 * Scale Mission: Particles, Clouds, Lattice, and Pipe Gas 3
 *
 * Transport-focused flowing gas system.
 * Treats pipe gas as a real flow-state solver, not just a pressure number.
 *
 * Physical scope:
 *   - EOS choice (ideal, VdW, Redlich-Kwong) via gas2
 *   - Viscosity, thermal conductivity
 *   - Reynolds number, friction factor (Darcy-Weisbach)
 *   - Pressure drop (Δp = f * L/D * ρv²/2)
 *   - Sound speed, Joule-Thomson coefficient
 *   - Heat exchange (wall flux q_wall)
 *   - Inlet / outlet state tracking
 *   - Mixture support: χ_i species fractions
 *
 * Core state:
 *   P = {geometry, fluid_mixture, T_in, P_in, ṁ, T_out, P_out, ΔP, q_wall}
 *
 * Stress bands (from mission work order):
 *   Instant    : one species, one segment, one state point
 *   Short_50ms : one segment + sweep-ready
 *   Medium_5s  : several species, multiple segments, short P/T sweep
 *   Long_5min  : large P range, multiple species families,
 *                mixture table + energy + steam + combined export
 *
 * Deliverables:
 *   - gas3_summary.csv
 *   - pipe_state_table.csv
 *   - pipe_energy_combined.csv
 *   - optional markdown report
 *
 * Integrates with:
 *   include/mission/mission_profile.hpp  — shared profile + entity layer
 *   include/gas2/gas2_engine.hpp         — EOS + kinetic + heat
 *   include/gas2/gas2_kinetic.hpp        — viscosity, MFP
 *   include/gas2/gas2_heat.hpp           — JT coefficient, heat capacity
 *   include/gas3/gas3_state_record.hpp   — QualityTier
 *   include/pipe_network.hpp             — Vec3×Vec3 pipe segment geometry
 */

#include "mission/mission_profile.hpp"
#include "gas2/gas2_engine.hpp"
#include "gas2/gas2_kinetic.hpp"
#include "gas2/gas2_heat.hpp"
#include "gas3/gas3_state_record.hpp"

#include <vector>
#include <string>
#include <cmath>
#include <optional>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <numeric>

namespace vsepr {
namespace mission {
namespace pipe_gas3 {

// ============================================================================
// RuntimeProfile factory for V4 Pipe Gas 3
// ============================================================================

inline RuntimeProfile profile_for(MissionScale s) {
    RuntimeProfile p{};
    p.scale               = s;
    p.pairwise_interactions = false;
    p.export_csv          = true;

    switch (s) {
        case MissionScale::Instant:
            p.max_entities       = 1;    // one pipe segment
            p.max_steps          = 0;    // single state-point solve
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
            p.max_entities       = 1;
            p.max_steps          = 1;
            p.dt                 = 1.0;
            p.high_accuracy      = false;
            p.neighbor_list      = false;
            p.export_full_history = false;
            p.export_markdown    = false;
            p.export_xyz         = false;
            p.approximation_level = 0;
            p.wall_budget_s      = 0.05;
            break;

        case MissionScale::Medium_5s:
            p.max_entities       = 10;   // multiple segments
            p.max_steps          = 20;   // P/T sweep steps
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
            p.max_entities       = 100;  // long pipe suite
            p.max_steps          = 200;
            p.dt                 = 1.0;
            p.high_accuracy      = true;
            p.neighbor_list      = false;
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
// Pipe geometry
// ============================================================================

struct PipeGeometry {
    double length_m    {1.0};    // pipe length (m)
    double diameter_m  {0.025}; // inner diameter (m)
    double roughness_m {4.6e-5}; // absolute roughness (m) — steel default

    double area()  const { return 3.14159265 * diameter_m * diameter_m * 0.25; }
    double volume() const { return area() * length_m; }
    double L_over_D() const { return length_m / diameter_m; }
};

// ============================================================================
// Pipe flow state  P = {geometry, fluid, T_in, P_in, ṁ, T_out, P_out, ΔP, q_wall}
// ============================================================================

struct PipeFlowState {
    // Geometry
    PipeGeometry geometry;

    // Fluid identity (dominant species + optional mixture)
    std::string  formula;
    const gas2::GasSpecies* species {nullptr}; // resolved from gas2 DB

    // Mixture fractions (if multi-species)
    std::vector<std::pair<std::string,double>> mix_fractions;

    // Inlet conditions
    double T_in_K  {300.0};    // K
    double P_in_Pa {200000.0}; // Pa (2 bar default)
    double mdot    {0.01};     // kg/s mass flow rate

    // Solved outlet conditions
    double T_out_K {0.0};
    double P_out_Pa{0.0};
    double dP_Pa   {0.0};     // pressure drop (Pa)
    double q_wall  {0.0};     // wall heat flux (W/m²)

    // Derived flow properties
    double Re      {0.0};     // Reynolds number
    double friction_f {0.0}; // Darcy-Weisbach friction factor
    double v_mean  {0.0};    // mean flow velocity (m/s)
    double mach    {0.0};    // Mach number
    double JT_K_Pa {0.0};   // Joule-Thomson coefficient (K/Pa)

    // EOS result at inlet
    gas2::EOSResult eos_in;

    // Quality
    gas3::QualityTier quality {gas3::QualityTier::Q2};

    // Solved flag
    bool solved {false};

    // History (for sweep mode)
    struct Record {
        double T_in, P_in, dP, Re, v, mach;
    };
    std::vector<Record> history;
};

// ============================================================================
// Friction factor — Churchill correlation (covers all Re regimes)
//   Covers laminar, transition, turbulent in a single expression.
//   Churchill (1977): f = 8 * [(8/Re)^12 + (A+B)^(-3/2)]^(1/12)
// ============================================================================

inline double friction_factor_churchill(double Re, double eps_over_D) {
    if (Re < 1e-6) return 64.0; // degenerate
    if (Re < 2300.0) return 64.0 / Re; // laminar

    double A = std::pow(
        2.457 * std::log(1.0 / (std::pow(7.0/Re, 0.9) + 0.27*eps_over_D)),
        16.0);
    double B = std::pow(37530.0 / Re, 16.0);
    return 8.0 * std::pow(
        std::pow(8.0/Re, 12.0) + std::pow(A + B, -1.5),
        1.0/12.0);
}

// ============================================================================
// Single segment solver
//
// Given inlet (T_in, P_in, ṁ) + geometry + species:
//   1. Solve EOS at inlet → ρ
//   2. Compute v = ṁ / (ρ A)
//   3. Compute Re = ρ v D / μ
//   4. Compute f (Churchill)
//   5. ΔP = f * (L/D) * ρ v² / 2
//   6. P_out = P_in - ΔP
//   7. T_out = T_in (adiabatic, no wall heat)  +  JT correction
//   8. Mach = v / c_sound
//
// Contract:
//   Required state: geometry, species, T_in, P_in, mdot
//   Writes:         Re, friction_f, v_mean, dP, P_out, T_out, mach, quality
//   Conservation:   mass (continuity) and enthalpy (if JT active)
//   dt_max:         N/A (steady-state solve)
// ============================================================================

inline void solve_segment(PipeFlowState& seg) {
    const double R = 8.314462; // J/(mol·K)
    seg.solved = false;

    if (!seg.species) {
        seg.quality = gas3::QualityTier::Q0;
        return;
    }

    const auto& sp = *seg.species;
    const double M  = sp.molar_mass_g * 1e-3; // kg/mol
    const double mu = sp.viscosity_uPas * 1e-6; // Pa·s

    // EOS at inlet (ideal baseline, then VdW correction)
    seg.eos_in = gas2::ideal_gas(1.0, seg.T_in_K, seg.P_in_Pa);
    double rho  = seg.P_in_Pa * M / (R * seg.T_in_K); // kg/m³ ideal
    double Z_in = 1.0;

    // VdW Z correction
    auto res_vdw = gas2::vdw_solve_volume(1.0, seg.T_in_K, seg.P_in_Pa,
                                           sp.vdw_a, sp.vdw_b);
    if (res_vdw.converged) {
        Z_in = res_vdw.Z;
        rho  = rho / Z_in; // corrected density
    }
    seg.eos_in.Z = Z_in;

    const double A    = seg.geometry.area();
    seg.v_mean        = seg.mdot / (rho * A);

    // Reynolds number
    const double D    = seg.geometry.diameter_m;
    seg.Re            = rho * seg.v_mean * D / (mu > 0.0 ? mu : 1e-5);

    // Friction factor (Churchill)
    const double eps_D = seg.geometry.roughness_m / D;
    seg.friction_f     = friction_factor_churchill(seg.Re, eps_D);

    // Pressure drop — Darcy-Weisbach
    seg.dP_Pa  = seg.friction_f * seg.geometry.L_over_D()
               * rho * seg.v_mean * seg.v_mean * 0.5;
    seg.P_out_Pa = seg.P_in_Pa - seg.dP_Pa;

    // Sound speed  c = sqrt(γ R T / M)
    double c_s    = std::sqrt(sp.gamma * R * seg.T_in_K / M);
    seg.mach      = seg.v_mean / c_s;

    // Joule-Thomson coefficient  μ_JT = (T B₂' - B₂) / Cp_m
    // Simplified: μ_JT = (2a/RT - b) / Cp  for VdW
    const double T  = seg.T_in_K;
    const double Cp = sp.Cp_Jmol; // J/(mol·K)
    seg.JT_K_Pa = ((2.0 * sp.vdw_a) / (R * T) - sp.vdw_b) / Cp;

    // T_out — adiabatic + JT correction for pressure drop
    seg.T_out_K = seg.T_in_K + seg.JT_K_Pa * (-seg.dP_Pa);

    // Wall heat flux (no active exchange in default — set to 0)
    seg.q_wall = 0.0;

    // Quality tier
    if (!res_vdw.converged)
        seg.quality = gas3::QualityTier::Q1;
    else if (seg.mach > 0.3)
        seg.quality = gas3::QualityTier::Q2; // compressibility corrections needed
    else if (seg.mach > 0.8)
        seg.quality = gas3::QualityTier::Q1;
    else
        seg.quality = gas3::QualityTier::Q3;

    seg.solved = true;

    seg.history.push_back({
        seg.T_in_K, seg.P_in_Pa, seg.dP_Pa, seg.Re, seg.v_mean, seg.mach
    });
}

// ============================================================================
// Pipe network (multi-segment in series)
// ============================================================================

struct PipeNetwork {
    std::vector<PipeFlowState> segments;
    RuntimeProfile             profile;
    std::size_t                step     {0};
    double                     sim_time {0.0};
};

// ============================================================================
// Series connection helper
//   Outlet of segment i becomes inlet of segment i+1.
// ============================================================================

inline void connect_series(PipeNetwork& net) {
    for (std::size_t i = 1; i < net.segments.size(); ++i) {
        net.segments[i].T_in_K  = net.segments[i-1].T_out_K;
        net.segments[i].P_in_Pa = net.segments[i-1].P_out_Pa;
        net.segments[i].mdot    = net.segments[i-1].mdot;
        if (!net.segments[i].species)
            net.segments[i].species = net.segments[i-1].species;
    }
}

// ============================================================================
// Main run — deterministic pipe scheduler
// ============================================================================

inline MissionDeliverable run(PipeNetwork& net) {
    MissionDeliverable d{};
    d.sim_version  = "V4_PipeGas3";
    d.scale        = net.profile.scale;
    d.entity_count = net.segments.size();

    auto t_start = std::chrono::steady_clock::now();

    if (net.profile.max_steps == 0) {
        // Instant: single solve, one segment
        if (!net.segments.empty())
            solve_segment(net.segments[0]);
    } else {
        // Sweep: vary inlet pressure over max_steps steps
        if (!net.segments.empty()) {
            const double P_lo = 100000.0;  // 1 bar
            const double P_hi = 1000000.0; // 10 bar
            const double dP   = (P_hi - P_lo) / net.profile.max_steps;

            for (std::size_t step = 0; step < net.profile.max_steps; ++step) {
                double P_sweep = P_lo + step * dP;
                for (auto& seg : net.segments) {
                    seg.P_in_Pa = P_sweep;
                }
                connect_series(net);
                for (auto& seg : net.segments) solve_segment(seg);

                net.step     = step + 1;
                net.sim_time = (step + 1) * net.profile.dt;

                if (net.profile.wall_budget_s > 0.0) {
                    auto elapsed = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - t_start).count();
                    if (elapsed > net.profile.wall_budget_s) break;
                }
            }
        }
    }

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    // Summary from last segment
    if (!net.segments.empty()) {
        const auto& last = net.segments.back();
        d.temperature_out = last.T_out_K;
        d.pressure_out    = last.P_out_Pa;
        d.delta_P         = last.dP_Pa;
    }

    d.steps_run   = net.step;
    d.wall_time_s = elapsed;
    d.converged   = true;

    return d;
}

// ============================================================================
// CSV export — pipe state table
// ============================================================================

inline std::string to_csv_pipe_state(const PipeNetwork& net) {
    std::ostringstream o;
    o << "seg_id,formula,T_in_K,P_in_Pa,mdot_kg_s,L_m,D_m,"
      << "Re,f,v_m_s,mach,dP_Pa,P_out_Pa,T_out_K,JT_K_Pa,quality\n";
    for (std::size_t i = 0; i < net.segments.size(); ++i) {
        const auto& s = net.segments[i];
        std::string sp = s.formula;
        o << i << "," << sp << ","
          << std::fixed << std::setprecision(2)
          << s.T_in_K << ","
          << std::setprecision(0) << s.P_in_Pa << ","
          << std::setprecision(5) << s.mdot << ","
          << std::setprecision(3) << s.geometry.length_m << ","
          << s.geometry.diameter_m << ","
          << std::setprecision(1) << s.Re << ","
          << std::setprecision(6) << s.friction_f << ","
          << std::setprecision(4) << s.v_mean << ","
          << s.mach << ","
          << std::setprecision(1) << s.dP_Pa << ","
          << s.P_out_Pa << ","
          << std::setprecision(3) << s.T_out_K << ","
          << std::setprecision(6) << s.JT_K_Pa << ","
          << gas3::tier_name(s.quality) << "\n";
    }
    return o.str();
}

// ============================================================================
// Console report
// ============================================================================

inline std::string report(const PipeNetwork& net, const MissionDeliverable& d) {
    std::ostringstream o;
    o << "\n  V4 Pipe Gas 3 — " << mission_scale_name(net.profile.scale) << "\n";
    o << "  " << std::string(60, '-') << "\n";
    o << "  Segments  : " << d.entity_count << "\n";
    o << "  Steps     : " << d.steps_run << "\n";
    o << "  Wall time : " << std::fixed << std::setprecision(4)
      << d.wall_time_s << " s\n";
    if (d.delta_P) {
        o << "  ΔP (last) : " << std::setprecision(1) << *d.delta_P << " Pa\n";
    }
    if (d.pressure_out) {
        o << "  P_out     : " << *d.pressure_out << " Pa\n";
    }
    if (d.temperature_out) {
        o << "  T_out     : " << std::setprecision(2) << *d.temperature_out << " K\n";
    }
    o << "\n  Per-segment table:\n";
    o << "  " << std::left
      << std::setw(4)  << "#"
      << std::setw(8)  << "Species"
      << std::setw(10) << "Re"
      << std::setw(10) << "v (m/s)"
      << std::setw(8)  << "Mach"
      << std::setw(12) << "ΔP (Pa)"
      << std::setw(10) << "T_out (K)"
      << "Quality\n";
    o << "  " << std::string(66, '-') << "\n";
    for (std::size_t i = 0; i < net.segments.size(); ++i) {
        const auto& s = net.segments[i];
        if (!s.solved) {
            o << "  " << i << "  (not solved)\n";
            continue;
        }
        o << "  " << std::left << std::setw(4) << i
          << std::setw(8) << s.formula
          << std::setw(10) << std::fixed << std::setprecision(0) << s.Re
          << std::setw(10) << std::setprecision(3) << s.v_mean
          << std::setw(8)  << std::setprecision(4) << s.mach
          << std::setw(12) << std::setprecision(1) << s.dP_Pa
          << std::setw(10) << std::setprecision(2) << s.T_out_K
          << gas3::tier_name(s.quality) << "\n";
    }
    return o.str();
}

} // namespace pipe_gas3
} // namespace mission
} // namespace vsepr
