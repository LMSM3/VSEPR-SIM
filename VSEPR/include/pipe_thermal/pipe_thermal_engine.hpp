#pragma once
/**
 * pipe_thermal_engine.hpp - Pipe Flow & Thermal Simulation Kernel
 *
 * Deterministic 1D pipe flow with thermal coupling, built on top of
 * the existing pipe_network infrastructure (pipe_network.hpp) and
 * atomistic thermal properties (thermal_properties.hpp).
 *
 * Physics:
 * --------
 * 1. Steady-state pipe flow (Hagen-Poiseuille for laminar, Darcy-Weisbach
 *    for turbulent via Colebrook-White friction factor):
 *
 *    Q = (pi * D^4 * dP) / (128 * mu * L)         [laminar, Re < 2300]
 *    dP = f * (L/D) * (rho * v^2 / 2)              [general]
 *
 * 2. Convective heat transfer (Dittus-Boelter for turbulent,
 *    Nu = 3.66 for laminar fully-developed):
 *
 *    q = h * A * (T_wall - T_fluid)
 *    h = Nu * k_f / D
 *
 * 3. Thermal conduction through pipe wall (cylindrical):
 *
 *    R_wall = ln(r_o/r_i) / (2*pi*k_wall*L)
 *
 * 4. Network-level energy balance (per-segment):
 *    T_out = T_wall - (T_wall - T_in) * exp(-h*pi*D*L / (m_dot*Cp))
 *
 * All computations use SI internally.  Unit conversions happen only at
 * the display boundary (see UnitConverter at bottom).
 *
 * Deterministic: same inputs -> bit-identical outputs.
 * Anti-black-box: every intermediate (Re, Nu, f, h, dP) is exposed.
 *
 * References:
 *   Incropera, F.P. et al. (2007). Fundamentals of Heat and Mass Transfer.
 *   White, F.M. (2011). Fluid Mechanics. 7th ed.
 *   Colebrook, C.F. (1939). J. Inst. Civil Eng. 11, 133-156.
 */

#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <random>

namespace pipe_thermal {

// ============================================================================
// Physical constants (SI)
// ============================================================================
constexpr double PI = 3.14159265358979323846;

// ============================================================================
// Fluid Properties
// ============================================================================
struct FluidProperties {
    std::string name = "Water";
    double density        = 997.0;    // kg/m^3
    double viscosity      = 8.9e-4;   // Pa*s (dynamic)
    double specific_heat  = 4186.0;   // J/(kg*K) = Cp
    double conductivity   = 0.607;    // W/(m*K)
    double prandtl() const {
        return (conductivity > 0) ? viscosity * specific_heat / conductivity : 0.0;
    }
};

// Common fluids (factory)
inline FluidProperties water_20C() {
    return {"Water (20C)", 998.2, 1.002e-3, 4182.0, 0.598};
}
inline FluidProperties water_80C() {
    return {"Water (80C)", 971.8, 3.545e-4, 4197.0, 0.670};
}
inline FluidProperties air_20C() {
    return {"Air (20C)", 1.204, 1.825e-5, 1007.0, 0.0257};
}
inline FluidProperties ethylene_glycol() {
    return {"Ethylene Glycol", 1113.0, 1.61e-2, 2382.0, 0.258};
}
inline FluidProperties engine_oil() {
    return {"Engine Oil (SAE 30)", 876.0, 0.290, 1964.0, 0.145};
}

// ============================================================================
// Pipe Material Properties
// ============================================================================
struct PipeMaterial {
    std::string name = "Copper";
    double conductivity    = 385.0;    // W/(m*K)
    double roughness       = 1.5e-6;   // m (surface roughness for friction)
    double density         = 8960.0;   // kg/m^3
    double specific_heat   = 385.0;    // J/(kg*K)
    double yield_strength  = 210e6;    // Pa
    double thermal_expansion = 16.5e-6; // 1/K
};

inline PipeMaterial copper_pipe()    { return {"Copper",    385.0,  1.5e-6, 8960, 385,  210e6, 16.5e-6}; }
inline PipeMaterial steel_pipe()     { return {"Steel",      50.2,  4.5e-5, 7850, 490,  250e6, 12.0e-6}; }
inline PipeMaterial pvc_pipe()       { return {"PVC",         0.16, 1.5e-6, 1400, 900,   45e6, 70.0e-6}; }
inline PipeMaterial stainless_pipe() { return {"Stainless",  16.3,  2.0e-6, 8000, 500,  205e6, 17.3e-6}; }
inline PipeMaterial aluminum_pipe()  { return {"Aluminum",  237.0,  1.5e-6, 2700, 900,  276e6, 23.1e-6}; }

// ============================================================================
// Per-Segment Flow & Thermal Result
// ============================================================================
struct SegmentResult {
    uint32_t segment_id = 0;

    // Geometry (SI)
    double length_m         = 0;
    double inner_diameter_m = 0;
    double outer_diameter_m = 0;
    double wall_thickness_m = 0;
    double flow_area_m2     = 0;
    double wetted_perimeter  = 0;

    // Flow
    double velocity_ms     = 0;   // mean flow velocity m/s
    double Re              = 0;   // Reynolds number
    bool   is_laminar      = true;
    double friction_factor = 0;   // Darcy friction factor
    double pressure_drop_Pa = 0;  // dP across segment
    double volume_flow_m3s = 0;   // Q = v*A
    double mass_flow_kgs   = 0;   // m_dot = rho*Q

    // Thermal
    double Nu              = 0;   // Nusselt number
    double h_conv          = 0;   // convection coefficient W/(m^2*K)
    double R_conv          = 0;   // convective resistance K/W
    double R_wall          = 0;   // wall conduction resistance K/W
    double R_total         = 0;   // total thermal resistance K/W
    double heat_transfer_W = 0;   // total heat exchanged (W)
    double T_fluid_in_K    = 0;
    double T_fluid_out_K   = 0;
    double T_wall_K        = 0;
    double LMTD            = 0;   // log-mean temperature difference

    // Derived
    double thermal_efficiency = 0; // (T_out - T_in) / (T_wall - T_in)
    double pumping_power_W   = 0; // dP * Q

    // Dimensionless groups and regime diagnostics
    double Pr              = 0;   // Prandtl number (fluid property)
    double St              = 0;   // Stanton number  St = Nu / (Re * Pr)
    double Pe              = 0;   // Peclet number   Pe = Re * Pr
    double Gz              = 0;   // Graetz number   Gz = Re * Pr * D / L
    double Bi_wall         = 0;   // Biot number (wall)  Bi = h * t_wall / k_wall
    double eps_D           = 0;   // Relative roughness  eps/D
    double hydraulic_d_m   = 0;   // Hydraulic diameter (= inner_d for circular pipe)
    double surface_area_m2 = 0;   // Wetted inner surface area A = pi*D*L
    double COP_segment     = 0;   // Heat-to-pumping ratio  Q_heat / W_pump
    double Re_star         = 0;   // Roughness Reynolds Re* = Re * eps_D
    std::string flow_regime; // "laminar", "transitional", "turbulent"
};

// ============================================================================
// Network-Level Result
// ============================================================================
struct NetworkResult {
    std::vector<SegmentResult> segments;

    // Primary energy / flow balance
    double total_pressure_drop_Pa = 0;
    double total_heat_transfer_W  = 0;
    double total_pumping_power_W  = 0;
    double T_inlet_K              = 0;
    double T_outlet_K             = 0;
    double network_effectiveness  = 0;

    // Regime counts
    uint32_t n_laminar      = 0;
    uint32_t n_turbulent    = 0;
    uint32_t n_transitional = 0;

    // Statistical aggregates across segments
    double Re_mean     = 0;  double Re_max     = 0;  double Re_min     = 1e30;
    double Nu_mean     = 0;  double Nu_max     = 0;
    double h_mean      = 0;  double h_max      = 0;
    double dP_max_seg  = 0;  // worst-case segment pressure drop
    double Q_max_seg   = 0;  // hottest segment heat load
    double eff_mean    = 0;  // mean per-segment thermal efficiency
    double COP_network = 0;  // total Q_heat / total W_pump
    double total_surface_area_m2 = 0;
    double total_length_m        = 0;
    double heat_flux_avg_Wm2     = 0;  // total Q / total surface area
    double dT_rise_K             = 0;  // T_outlet - T_inlet
    double specific_heat_kJkg    = 0;  // kJ of heat per kg of fluid

    // Fluid / material snapshot for traceability
    std::string fluid_name;
    std::string material_name;

    std::string summary() const {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4);
        os << "=== Pipe Flow & Thermal Network Result ===\n";
        os << "Fluid:          " << fluid_name << "\n";
        os << "Material:       " << material_name << "\n";
        os << "Segments:       " << segments.size()
           << "  (L" << n_laminar << " | Tr" << n_transitional << " | Tu" << n_turbulent << ")\n";
        os << "Total length:   " << std::setprecision(3) << total_length_m << " m\n";
        os << "Surface area:   " << std::setprecision(4) << total_surface_area_m2 << " m^2\n";
        os << "Total dP:       " << std::setprecision(2) << total_pressure_drop_Pa << " Pa  ("
           << total_pressure_drop_Pa * 1e-3 << " kPa  /  "
           << total_pressure_drop_Pa * 1.45038e-4 << " psi  /  "
           << total_pressure_drop_Pa * 1e-5 << " bar)\n";
        os << "Total Q_heat:   " << total_heat_transfer_W << " W  ("
           << total_heat_transfer_W * 1e-3 << " kW  /  "
           << total_heat_transfer_W * 3.41214 << " BTU/hr)\n";
        os << "Pumping power:  " << total_pumping_power_W << " W\n";
        os << "COP (Q/W_pump): " << std::setprecision(2) << COP_network << "\n";
        os << "Heat flux avg:  " << std::setprecision(1) << heat_flux_avg_Wm2 << " W/m^2\n";
        os << "T_inlet:        " << std::setprecision(2) << (T_inlet_K - 273.15) << " C  (" << T_inlet_K << " K)\n";
        os << "T_outlet:       " << (T_outlet_K - 273.15) << " C  (" << T_outlet_K << " K)\n";
        os << "dT_rise:        " << dT_rise_K << " K\n";
        os << "Effectiveness:  " << std::setprecision(4) << network_effectiveness << "\n";
        os << "Re  mean/max:   " << std::setprecision(1) << Re_mean << " / " << Re_max << "\n";
        os << "Nu  mean/max:   " << std::setprecision(2) << Nu_mean << " / " << Nu_max << "\n";
        os << "h   mean/max:   " << std::setprecision(1) << h_mean << " / " << h_max << " W/(m^2 K)\n";
        os << "dP  worst seg:  " << std::setprecision(1) << dP_max_seg << " Pa\n";
        os << "Q   hottest:    " << Q_max_seg << " W\n";
        os << "Eff mean:       " << std::setprecision(4) << eff_mean << "\n";
        return os.str();
    }

    // Structured diagnostic block — fully traceable, inspectable per-segment detail
    std::string diagnostic_block() const {
        std::ostringstream os;
        os << std::fixed;
        os << "\n--- Segment Diagnostic Table ---\n";
        os << std::left
           << std::setw(4)  << "Seg"
           << std::setw(12) << "Regime"
           << std::setw(9)  << "Re"
           << std::setw(10) << "Pr"
           << std::setw(8)  << "Pe"
           << std::setw(8)  << "Gz"
           << std::setw(8)  << "St(e-3)"
           << std::setw(8)  << "Bi_wall"
           << std::setw(8)  << "eps/D"
           << std::setw(8)  << "Nu"
           << std::setw(10) << "h(W/m2K)"
           << std::setw(10) << "LMTD(K)"
           << std::setw(10) << "eff"
           << std::setw(10) << "COP"
           << "\n";
        os << std::string(125, '-') << "\n";
        for (const auto& s : segments) {
            os << std::setprecision(0)
               << std::setw(4)  << s.segment_id
               << std::left
               << std::setw(12) << s.flow_regime
               << std::setprecision(0) << std::setw(9) << s.Re
               << std::setprecision(3) << std::setw(10) << s.Pr
               << std::setprecision(1) << std::setw(8) << s.Pe
               << std::setprecision(1) << std::setw(8) << s.Gz
               << std::setprecision(3) << std::setw(8) << (s.St * 1e3)
               << std::setprecision(4) << std::setw(8) << s.Bi_wall
               << std::setprecision(5) << std::setw(8) << s.eps_D
               << std::setprecision(2) << std::setw(8) << s.Nu
               << std::setprecision(1) << std::setw(10) << s.h_conv
               << std::setprecision(2) << std::setw(10) << s.LMTD
               << std::setprecision(4) << std::setw(10) << s.thermal_efficiency
               << std::setprecision(2) << std::setw(10) << s.COP_segment
               << "\n";
        }
        os << "\n--- Thermal Resistance Breakdown ---\n";
        os << std::left
           << std::setw(4)  << "Seg"
           << std::setw(14) << "R_conv(K/W)"
           << std::setw(14) << "R_wall(K/W)"
           << std::setw(14) << "R_total(K/W)"
           << std::setw(14) << "R_conv/R_tot"
           << std::setw(12) << "Area(m^2)"
           << std::setw(12) << "Q_flux(W/m2)"
           << "\n";
        os << std::string(84, '-') << "\n";
        for (const auto& s : segments) {
            double r_frac  = (s.R_total > 1e-30) ? s.R_conv / s.R_total : 0.0;
            double q_flux  = (s.surface_area_m2 > 1e-30) ? s.heat_transfer_W / s.surface_area_m2 : 0.0;
            os << std::setw(4)  << s.segment_id
               << std::setprecision(4)
               << std::setw(14) << s.R_conv
               << std::setw(14) << s.R_wall
               << std::setw(14) << s.R_total
               << std::setprecision(3)
               << std::setw(14) << r_frac
               << std::setprecision(5)
               << std::setw(12) << s.surface_area_m2
               << std::setprecision(1)
               << std::setw(12) << q_flux
               << "\n";
        }
        return os.str();
    }
};

// ============================================================================
// Friction Factor (Colebrook-White, iterative)
// ============================================================================
inline double colebrook_friction(double Re, double eps_D) {
    if (Re <= 0) return 0;
    if (Re < 2300) {
        return 64.0 / Re;  // Laminar
    }
    // Churchill approximation (explicit, avoids iteration)
    double A = std::pow(2.457 * std::log(1.0 / (std::pow(7.0/Re, 0.9) + 0.27*eps_D)), 16.0);
    double B = std::pow(37530.0 / Re, 16.0);
    double f = 8.0 * std::pow(std::pow(8.0/Re, 12.0) + 1.0/std::pow(A+B, 1.5), 1.0/12.0);
    return f;
}

// ============================================================================
// Nusselt Number
// ============================================================================
inline double nusselt_number(double Re, double Pr, bool laminar) {
    if (laminar) {
        return 3.66;  // Fully-developed laminar, constant wall temperature
    }
    // Dittus-Boelter (heating): Nu = 0.023 * Re^0.8 * Pr^0.4
    if (Re < 1 || Pr < 0.01) return 3.66;
    return 0.023 * std::pow(Re, 0.8) * std::pow(Pr, 0.4);
}

// ============================================================================
// Pipe Thermal Engine — Single Segment
// ============================================================================
inline SegmentResult solve_segment(
    double length_m,
    double inner_d_m,
    double outer_d_m,
    double inlet_velocity_ms,
    double T_fluid_in_K,
    double T_wall_K,
    const FluidProperties& fluid,
    const PipeMaterial& material)
{
    SegmentResult r;
    r.length_m = length_m;
    r.inner_diameter_m = inner_d_m;
    r.outer_diameter_m = outer_d_m;
    r.wall_thickness_m = (outer_d_m - inner_d_m) / 2.0;
    r.flow_area_m2 = PI * inner_d_m * inner_d_m / 4.0;
    r.wetted_perimeter = PI * inner_d_m;
    r.velocity_ms = inlet_velocity_ms;
    r.T_fluid_in_K = T_fluid_in_K;
    r.T_wall_K = T_wall_K;

    if (inner_d_m <= 0 || length_m <= 0) return r;

    // Reynolds number
    r.Re = fluid.density * inlet_velocity_ms * inner_d_m / fluid.viscosity;
    r.is_laminar = (r.Re < 2300.0);

    // Friction factor
    double eps_D = material.roughness / inner_d_m;
    r.friction_factor = colebrook_friction(r.Re, eps_D);

    // Pressure drop: dP = f * (L/D) * (rho*v^2/2)
    double v2 = inlet_velocity_ms * inlet_velocity_ms;
    r.pressure_drop_Pa = r.friction_factor * (length_m / inner_d_m) * (fluid.density * v2 / 2.0);

    // Flow rates
    r.volume_flow_m3s = inlet_velocity_ms * r.flow_area_m2;
    r.mass_flow_kgs = fluid.density * r.volume_flow_m3s;

    // Nusselt number and convection coefficient
    double Pr = fluid.prandtl();
    r.Nu = nusselt_number(r.Re, Pr, r.is_laminar);
    r.h_conv = (inner_d_m > 0) ? r.Nu * fluid.conductivity / inner_d_m : 0;

    // Thermal resistances
    double A_inner = PI * inner_d_m * length_m;
    r.R_conv = (r.h_conv > 0 && A_inner > 0) ? 1.0 / (r.h_conv * A_inner) : 1e20;

    double r_o = outer_d_m / 2.0;
    double r_i = inner_d_m / 2.0;
    if (r_o > r_i && material.conductivity > 0) {
        r.R_wall = std::log(r_o / r_i) / (2.0 * PI * material.conductivity * length_m);
    }
    r.R_total = r.R_conv + r.R_wall;

    // Outlet temperature (exponential model for constant wall temp)
    // T_out = T_wall - (T_wall - T_in) * exp(-h*P*L / (m_dot*Cp))
    if (r.mass_flow_kgs > 0 && fluid.specific_heat > 0) {
        double NTU = r.h_conv * r.wetted_perimeter * length_m / (r.mass_flow_kgs * fluid.specific_heat);
        r.T_fluid_out_K = T_wall_K - (T_wall_K - T_fluid_in_K) * std::exp(-NTU);
    } else {
        r.T_fluid_out_K = T_fluid_in_K;
    }

    // Heat transfer
    r.heat_transfer_W = r.mass_flow_kgs * fluid.specific_heat * (r.T_fluid_out_K - T_fluid_in_K);

    // LMTD
    double dT1 = T_wall_K - T_fluid_in_K;
    double dT2 = T_wall_K - r.T_fluid_out_K;
    if (dT1 > 0 && dT2 > 0 && std::abs(dT1 - dT2) > 1e-10) {
        r.LMTD = (dT1 - dT2) / std::log(dT1 / dT2);
    } else {
        r.LMTD = (dT1 + dT2) / 2.0;
    }

    // Efficiency
    if (std::abs(T_wall_K - T_fluid_in_K) > 1e-10) {
        r.thermal_efficiency = (r.T_fluid_out_K - T_fluid_in_K) / (T_wall_K - T_fluid_in_K);
    }

    // Pumping power
    r.pumping_power_W = r.pressure_drop_Pa * r.volume_flow_m3s;

    // Dimensionless groups
    r.Pr    = fluid.prandtl();
    r.Pe    = r.Re * r.Pr;
    r.St    = (r.Pe > 1e-30) ? r.Nu / r.Pe : 0.0;
    r.Gz    = (length_m > 1e-30) ? r.Re * r.Pr * inner_d_m / length_m : 0.0;
    r.Bi_wall = (material.conductivity > 1e-30 && r.wall_thickness_m > 0)
                ? r.h_conv * r.wall_thickness_m / material.conductivity : 0.0;
    r.eps_D = material.roughness / inner_d_m;
    r.Re_star = r.Re * r.eps_D;
    r.hydraulic_d_m   = inner_d_m;
    r.surface_area_m2 = PI * inner_d_m * length_m;

    // COP for this segment
    r.COP_segment = (r.pumping_power_W > 1e-30) ? r.heat_transfer_W / r.pumping_power_W : 0.0;

    // Flow regime string
    if (r.Re < 2300.0)       r.flow_regime = "laminar";
    else if (r.Re < 4000.0)  r.flow_regime = "transitional";
    else                     r.flow_regime = "turbulent";

    return r;
}

// ============================================================================
// Network Configuration
// ============================================================================
struct PipeThermalConfig {
    // Pipe geometry
    uint32_t n_segments          = 5;
    double   segment_length_m    = 2.0;
    double   inner_diameter_m    = 0.025;  // 25 mm
    double   outer_diameter_m    = 0.030;  // 30 mm

    // Flow conditions
    double   inlet_velocity_ms   = 1.0;
    double   T_inlet_K           = 293.15; // 20 C
    double   T_wall_K            = 373.15; // 100 C

    // Materials
    FluidProperties fluid = water_20C();
    PipeMaterial    material = copper_pipe();

    // Stochastic network generation
    bool     use_network         = false;
    uint64_t seed                = 42;
    double   chaos_factor        = 1.0;
};

// ============================================================================
// Solve entire series network (segments in series)
// ============================================================================
inline NetworkResult solve_series_network(const PipeThermalConfig& cfg) {
    NetworkResult net;
    net.T_inlet_K = cfg.T_inlet_K;

    double T_in = cfg.T_inlet_K;

    for (uint32_t i = 0; i < cfg.n_segments; ++i) {
        SegmentResult seg = solve_segment(
            cfg.segment_length_m,
            cfg.inner_diameter_m,
            cfg.outer_diameter_m,
            cfg.inlet_velocity_ms,
            T_in,
            cfg.T_wall_K,
            cfg.fluid,
            cfg.material
        );
        seg.segment_id = i;

        net.total_pressure_drop_Pa += seg.pressure_drop_Pa;
        net.total_heat_transfer_W  += seg.heat_transfer_W;
        net.total_pumping_power_W  += seg.pumping_power_W;

        if      (seg.Re < 2300.0) net.n_laminar++;
        else if (seg.Re < 4000.0) net.n_transitional++;
        else                      net.n_turbulent++;

        T_in = seg.T_fluid_out_K;
        net.segments.push_back(seg);
    }

    net.T_outlet_K = T_in;
    double dT_max = cfg.T_wall_K - cfg.T_inlet_K;
    if (std::abs(dT_max) > 1e-10) {
        net.network_effectiveness = (net.T_outlet_K - cfg.T_inlet_K) / dT_max;
    }

    // Aggregate statistics
    net.fluid_name    = cfg.fluid.name;
    net.material_name = cfg.material.name;
    net.dT_rise_K     = net.T_outlet_K - cfg.T_inlet_K;
    if (!net.segments.empty()) {
        double sum_Re = 0, sum_Nu = 0, sum_h = 0, sum_eff = 0;
        for (const auto& s : net.segments) {
            sum_Re  += s.Re;  net.Re_max = std::max(net.Re_max, s.Re);
            sum_Nu  += s.Nu;  net.Nu_max = std::max(net.Nu_max, s.Nu);
            sum_h   += s.h_conv; net.h_max = std::max(net.h_max, s.h_conv);
            sum_eff += s.thermal_efficiency;
            net.dP_max_seg  = std::max(net.dP_max_seg, s.pressure_drop_Pa);
            net.Q_max_seg   = std::max(net.Q_max_seg,  s.heat_transfer_W);
            net.total_surface_area_m2 += s.surface_area_m2;
            net.total_length_m        += s.length_m;
            net.Re_min = std::min(net.Re_min, s.Re);
        }
        auto n = static_cast<double>(net.segments.size());
        net.Re_mean   = sum_Re  / n;
        net.Nu_mean   = sum_Nu  / n;
        net.h_mean    = sum_h   / n;
        net.eff_mean  = sum_eff / n;
    }
    if (net.total_pumping_power_W > 1e-30)
        net.COP_network = net.total_heat_transfer_W / net.total_pumping_power_W;
    if (net.total_surface_area_m2 > 1e-30)
        net.heat_flux_avg_Wm2 = net.total_heat_transfer_W / net.total_surface_area_m2;
    if (cfg.fluid.specific_heat > 0 && net.total_heat_transfer_W > 0) {
        double m_dot = cfg.fluid.density * cfg.inlet_velocity_ms
                     * (PI * cfg.inner_diameter_m * cfg.inner_diameter_m / 4.0);
        if (m_dot > 1e-30)
            net.specific_heat_kJkg = net.total_heat_transfer_W / m_dot / 1000.0;
    }

    return net;
}

// ============================================================================
// Generate stochastic pipe network and solve
// ============================================================================
inline NetworkResult solve_stochastic_network(const PipeThermalConfig& cfg) {
    std::mt19937_64 rng(cfg.seed);
    std::lognormal_distribution<double> len_dist(std::log(cfg.segment_length_m), 0.3);
    std::normal_distribution<double>    dia_noise(0.0, 0.002);

    NetworkResult net;
    net.T_inlet_K = cfg.T_inlet_K;
    double T_in = cfg.T_inlet_K;

    for (uint32_t i = 0; i < cfg.n_segments; ++i) {
        double L = std::max(0.1, len_dist(rng));
        double id = std::max(0.005, cfg.inner_diameter_m + dia_noise(rng));
        double od = id + 2.0 * (cfg.outer_diameter_m - cfg.inner_diameter_m) / 2.0;

        SegmentResult seg = solve_segment(
            L, id, od,
            cfg.inlet_velocity_ms,
            T_in,
            cfg.T_wall_K,
            cfg.fluid,
            cfg.material
        );
        seg.segment_id = i;

        net.total_pressure_drop_Pa += seg.pressure_drop_Pa;
        net.total_heat_transfer_W  += seg.heat_transfer_W;
        net.total_pumping_power_W  += seg.pumping_power_W;

        if      (seg.Re < 2300.0) net.n_laminar++;
        else if (seg.Re < 4000.0) net.n_transitional++;
        else                      net.n_turbulent++;

        T_in = seg.T_fluid_out_K;
        net.segments.push_back(seg);
    }

    net.T_outlet_K = T_in;
    double dT_max_s = cfg.T_wall_K - cfg.T_inlet_K;
    if (std::abs(dT_max_s) > 1e-10) {
        net.network_effectiveness = (net.T_outlet_K - cfg.T_inlet_K) / dT_max_s;
    }

    net.fluid_name    = cfg.fluid.name;
    net.material_name = cfg.material.name;
    net.dT_rise_K     = net.T_outlet_K - cfg.T_inlet_K;
    if (!net.segments.empty()) {
        double sum_Re = 0, sum_Nu = 0, sum_h = 0, sum_eff = 0;
        for (const auto& s : net.segments) {
            sum_Re  += s.Re;  net.Re_max = std::max(net.Re_max, s.Re);
            sum_Nu  += s.Nu;  net.Nu_max = std::max(net.Nu_max, s.Nu);
            sum_h   += s.h_conv; net.h_max = std::max(net.h_max, s.h_conv);
            sum_eff += s.thermal_efficiency;
            net.dP_max_seg  = std::max(net.dP_max_seg, s.pressure_drop_Pa);
            net.Q_max_seg   = std::max(net.Q_max_seg,  s.heat_transfer_W);
            net.total_surface_area_m2 += s.surface_area_m2;
            net.total_length_m        += s.length_m;
            net.Re_min = std::min(net.Re_min, s.Re);
        }
        auto n = static_cast<double>(net.segments.size());
        net.Re_mean   = sum_Re  / n;
        net.Nu_mean   = sum_Nu  / n;
        net.h_mean    = sum_h   / n;
        net.eff_mean  = sum_eff / n;
    }
    if (net.total_pumping_power_W > 1e-30)
        net.COP_network = net.total_heat_transfer_W / net.total_pumping_power_W;
    if (net.total_surface_area_m2 > 1e-30)
        net.heat_flux_avg_Wm2 = net.total_heat_transfer_W / net.total_surface_area_m2;
    if (cfg.fluid.specific_heat > 0 && net.total_heat_transfer_W > 0) {
        double m_dot_s = cfg.fluid.density * cfg.inlet_velocity_ms
                       * (PI * cfg.inner_diameter_m * cfg.inner_diameter_m / 4.0);
        if (m_dot_s > 1e-30)
            net.specific_heat_kJkg = net.total_heat_transfer_W / m_dot_s / 1000.0;
    }

    return net;
}

// ============================================================================
// Parametric Sweep — vary one parameter, return a table
// ============================================================================
struct SweepPoint {
    double parameter_value;
    NetworkResult result;
};

inline std::vector<SweepPoint> sweep_velocity(
    PipeThermalConfig cfg,
    double v_min, double v_max, int n_points)
{
    std::vector<SweepPoint> pts;
    for (int i = 0; i < n_points; ++i) {
        double v = v_min + (v_max - v_min) * i / std::max(1, n_points - 1);
        cfg.inlet_velocity_ms = v;
        pts.push_back({v, solve_series_network(cfg)});
    }
    return pts;
}

inline std::vector<SweepPoint> sweep_diameter(
    PipeThermalConfig cfg,
    double d_min, double d_max, int n_points)
{
    std::vector<SweepPoint> pts;
    double wall = (cfg.outer_diameter_m - cfg.inner_diameter_m) / 2.0;
    for (int i = 0; i < n_points; ++i) {
        double d = d_min + (d_max - d_min) * i / std::max(1, n_points - 1);
        cfg.inner_diameter_m = d;
        cfg.outer_diameter_m = d + 2.0 * wall;
        pts.push_back({d, solve_series_network(cfg)});
    }
    return pts;
}

inline std::vector<SweepPoint> sweep_wall_temperature(
    PipeThermalConfig cfg,
    double t_min, double t_max, int n_points)
{
    std::vector<SweepPoint> pts;
    for (int i = 0; i < n_points; ++i) {
        double t = t_min + (t_max - t_min) * i / std::max(1, n_points - 1);
        cfg.T_wall_K = t;
        pts.push_back({t, solve_series_network(cfg)});
    }
    return pts;
}

inline std::vector<SweepPoint> sweep_segments(
    PipeThermalConfig cfg,
    int n_min, int n_max)
{
    std::vector<SweepPoint> pts;
    for (int n = n_min; n <= n_max; ++n) {
        cfg.n_segments = static_cast<uint32_t>(n);
        pts.push_back({static_cast<double>(n), solve_series_network(cfg)});
    }
    return pts;
}

// ============================================================================
// Unit Conversion — display-end ONLY, never in kernel
// ============================================================================
namespace units {

// Temperature
inline double K_to_C(double K)  { return K - 273.15; }
inline double C_to_K(double C)  { return C + 273.15; }
inline double K_to_F(double K)  { return (K - 273.15) * 9.0/5.0 + 32.0; }
inline double F_to_K(double F)  { return (F - 32.0) * 5.0/9.0 + 273.15; }

// Pressure
inline double Pa_to_kPa(double Pa)  { return Pa * 1e-3; }
inline double Pa_to_bar(double Pa)  { return Pa * 1e-5; }
inline double Pa_to_psi(double Pa)  { return Pa * 1.45038e-4; }
inline double Pa_to_atm(double Pa)  { return Pa / 101325.0; }

// Length
inline double m_to_mm(double m)    { return m * 1000.0; }
inline double m_to_in(double m)    { return m * 39.3701; }
inline double m_to_ft(double m)    { return m * 3.28084; }

// Flow
inline double m3s_to_Lmin(double m3s) { return m3s * 60000.0; }
inline double m3s_to_gpm(double m3s)  { return m3s * 15850.3; }

// Power
inline double W_to_kW(double W)    { return W * 1e-3; }
inline double W_to_BTUhr(double W) { return W * 3.41214; }

} // namespace units

// ============================================================================
// Markdown Report Generator
// ============================================================================
inline std::string generate_report(const NetworkResult& net, const PipeThermalConfig& cfg) {
    std::ostringstream os;
    os << std::fixed;

    os << "# Pipe Flow & Thermal Analysis Report\n\n";
    os << "## Configuration\n\n";
    os << "| Parameter | Value |\n";
    os << "|-----------|-------|\n";
    os << "| Segments | " << cfg.n_segments << " |\n";
    os << std::setprecision(4);
    os << "| Segment Length | " << cfg.segment_length_m << " m |\n";
    os << "| Inner Diameter | " << units::m_to_mm(cfg.inner_diameter_m) << " mm |\n";
    os << "| Outer Diameter | " << units::m_to_mm(cfg.outer_diameter_m) << " mm |\n";
    os << std::setprecision(2);
    os << "| Inlet Velocity | " << cfg.inlet_velocity_ms << " m/s |\n";
    os << "| T_inlet | " << units::K_to_C(cfg.T_inlet_K) << " C (" << cfg.T_inlet_K << " K) |\n";
    os << "| T_wall | " << units::K_to_C(cfg.T_wall_K) << " C (" << cfg.T_wall_K << " K) |\n";
    os << "| Fluid | " << cfg.fluid.name << " |\n";
    os << "| Material | " << cfg.material.name << " |\n";
    os << "\n";

    os << "## Network Summary\n\n";
    os << "| Metric | SI | Converted |\n";
    os << "|--------|----|-----------|\n";
    os << std::setprecision(2);
    os << "| Total Pressure Drop | " << net.total_pressure_drop_Pa << " Pa | "
       << units::Pa_to_kPa(net.total_pressure_drop_Pa) << " kPa |\n";
    os << "| Total Heat Transfer | " << net.total_heat_transfer_W << " W | "
       << units::W_to_kW(net.total_heat_transfer_W) << " kW |\n";
    os << "| Pumping Power | " << net.total_pumping_power_W << " W | "
       << units::W_to_kW(net.total_pumping_power_W) << " kW |\n";
    os << "| T_outlet | " << net.T_outlet_K << " K | "
       << units::K_to_C(net.T_outlet_K) << " C |\n";
    os << std::setprecision(4);
    os << "| Effectiveness | " << net.network_effectiveness << " | - |\n";
    os << "| Laminar/Turbulent | " << net.n_laminar << "/" << net.n_turbulent << " | - |\n";
    os << "\n";

    os << "## Per-Segment Detail\n\n";
    os << "| Seg | L(m) | D_i(mm) | Re | f | dP(Pa) | Nu | h(W/m2K) | T_in(C) | T_out(C) | Q(W) |\n";
    os << "|-----|------|---------|-----|---|--------|----|---------|---------|---------|---------|\n";
    for (const auto& s : net.segments) {
        os << std::setprecision(3);
        os << "| " << s.segment_id
           << " | " << s.length_m
           << " | " << units::m_to_mm(s.inner_diameter_m)
           << " | " << std::setprecision(0) << s.Re
           << " | " << std::setprecision(6) << s.friction_factor
           << " | " << std::setprecision(1) << s.pressure_drop_Pa
           << " | " << std::setprecision(2) << s.Nu
           << " | " << std::setprecision(1) << s.h_conv
           << " | " << std::setprecision(2) << units::K_to_C(s.T_fluid_in_K)
           << " | " << units::K_to_C(s.T_fluid_out_K)
           << " | " << std::setprecision(1) << s.heat_transfer_W
           << " |\n";
    }
    os << "\n";

    return os.str();
}

// ============================================================================
// CSV Export
// ============================================================================
inline std::string export_csv(const NetworkResult& net) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(6);
    // Full column export — every computed intermediate exposed
    os << "segment_id,regime,length_m,inner_d_m,outer_d_m,wall_t_m,flow_area_m2,"
          "velocity_ms,Re,Pr,Pe,Gz,St,Bi_wall,eps_D,Re_star,"
          "friction_factor,dP_Pa,vol_flow_m3s,mass_flow_kgs,"
          "Nu,h_Wm2K,R_conv_KW,R_wall_KW,R_total_KW,"
          "T_in_K,T_out_K,T_wall_K,LMTD_K,"
          "Q_W,Q_flux_Wm2,eff,COP_seg,pumping_W,"
          "surface_area_m2,hydraulic_d_m\n";
    for (const auto& s : net.segments) {
        double q_flux = (s.surface_area_m2 > 1e-30) ? s.heat_transfer_W / s.surface_area_m2 : 0.0;
        os << s.segment_id    << ","
           << s.flow_regime   << ","
           << s.length_m      << ","
           << s.inner_diameter_m << ","
           << s.outer_diameter_m << ","
           << s.wall_thickness_m << ","
           << s.flow_area_m2  << ","
           << s.velocity_ms   << ","
           << s.Re            << ","
           << s.Pr            << ","
           << s.Pe            << ","
           << s.Gz            << ","
           << s.St            << ","
           << s.Bi_wall       << ","
           << s.eps_D         << ","
           << s.Re_star       << ","
           << s.friction_factor << ","
           << s.pressure_drop_Pa << ","
           << s.volume_flow_m3s << ","
           << s.mass_flow_kgs << ","
           << s.Nu            << ","
           << s.h_conv        << ","
           << s.R_conv        << ","
           << s.R_wall        << ","
           << s.R_total       << ","
           << s.T_fluid_in_K  << ","
           << s.T_fluid_out_K << ","
           << s.T_wall_K      << ","
           << s.LMTD          << ","
           << s.heat_transfer_W << ","
           << q_flux          << ","
           << s.thermal_efficiency << ","
           << s.COP_segment   << ","
           << s.pumping_power_W << ","
           << s.surface_area_m2 << ","
           << s.hydraulic_d_m << "\n";
    }
    return os.str();
}

} // namespace pipe_thermal
