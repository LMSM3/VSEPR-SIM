/**
 * pipe_thermal_tui.cpp - Interactive Pipe Flow & Thermal TUI
 *
 * Terminal-based interactive interface for the pipe flow and thermal
 * simulation engine.  Supports:
 *   - Interactive parameter editing
 *   - Fluid and material selection
 *   - Series and stochastic network modes
 *   - Parametric sweeps (velocity, diameter, temperature)
 *   - Markdown report generation
 *   - CSV data export
 *   - Deterministic reproducibility (seed control)
 *
 * All internal computations in SI.  Unit conversions at display boundary only.
 */

#include "include/pipe_thermal/pipe_thermal_engine.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <limits>

using namespace pipe_thermal;

static void print_header() {
    std::cout << "\n";
    std::cout << "=============================================================\n";
    std::cout << "  VSEPR-SIM  Pipe Flow & Thermal Analysis Module\n";
    std::cout << "  Deterministic | Anti-Black-Box | Research-Ready\n";
    std::cout << "=============================================================\n";
    std::cout << "\n";
}

static void print_config(const PipeThermalConfig& cfg) {
    double wall_mm = units::m_to_mm((cfg.outer_diameter_m - cfg.inner_diameter_m) / 2.0);
    std::cout << "\n=== Current Configuration ===\n";
    std::cout << "  Geometry:\n";
    std::cout << "    Segments:        " << cfg.n_segments << "\n";
    std::cout << "    Segment length:  " << cfg.segment_length_m << " m  ("
              << units::m_to_ft(cfg.segment_length_m) << " ft)\n";
    std::cout << "    Total length:    " << cfg.segment_length_m * cfg.n_segments << " m\n";
    std::cout << "    Inner diameter:  " << units::m_to_mm(cfg.inner_diameter_m) << " mm  ("
              << units::m_to_in(cfg.inner_diameter_m) << " in)\n";
    std::cout << "    Outer diameter:  " << units::m_to_mm(cfg.outer_diameter_m) << " mm\n";
    std::cout << "    Wall thickness:  " << wall_mm << " mm\n";
    std::cout << "  Flow Conditions:\n";
    std::cout << "    Inlet velocity:  " << cfg.inlet_velocity_ms << " m/s\n";
    std::cout << "    T_inlet:         " << units::K_to_C(cfg.T_inlet_K) << " C  ("
              << cfg.T_inlet_K << " K / " << units::K_to_F(cfg.T_inlet_K) << " F)\n";
    std::cout << "    T_wall:          " << units::K_to_C(cfg.T_wall_K) << " C  ("
              << cfg.T_wall_K << " K / " << units::K_to_F(cfg.T_wall_K) << " F)\n";
    std::cout << "    dT (wall-inlet): " << (cfg.T_wall_K - cfg.T_inlet_K) << " K\n";
    std::cout << "  Fluid: " << cfg.fluid.name << "\n";
    std::cout << "    rho:   " << cfg.fluid.density << " kg/m^3\n";
    std::cout << "    mu:    " << cfg.fluid.viscosity << " Pa*s\n";
    std::cout << "    Cp:    " << cfg.fluid.specific_heat << " J/(kg*K)\n";
    std::cout << "    k_f:   " << cfg.fluid.conductivity << " W/(m*K)\n";
    std::cout << "    Pr:    " << cfg.fluid.prandtl() << "\n";
    std::cout << "  Material: " << cfg.material.name << "\n";
    std::cout << "    k_wall:   " << cfg.material.conductivity << " W/(m*K)\n";
    std::cout << "    roughness:" << cfg.material.roughness * 1e6 << " um  (eps/D="
              << cfg.material.roughness / cfg.inner_diameter_m << ")\n";
    std::cout << "    rho:      " << cfg.material.density << " kg/m^3\n";
    std::cout << "    sigma_y:  " << cfg.material.yield_strength / 1e6 << " MPa\n";
    std::cout << "    alpha:    " << cfg.material.thermal_expansion * 1e6 << " um/(m*K)\n";
    std::cout << "  Simulation:\n";
    std::cout << "    Stochastic: " << (cfg.use_network ? "ON" : "OFF")
              << "  (seed=" << cfg.seed << ")\n";
    std::cout << "\n";
}

static void print_result(const NetworkResult& net) {
    std::cout << "\n";
    std::cout << "=== Network Results ===\n";
    std::cout << "  Fluid:            " << net.fluid_name << "\n";
    std::cout << "  Material:         " << net.material_name << "\n";
    std::cout << "  Segments:         " << net.segments.size()
              << "  [L=" << net.n_laminar
              << " Tr=" << net.n_transitional
              << " Tu=" << net.n_turbulent << "]\n";
    std::cout << "  Total length:     " << net.total_length_m << " m\n";
    std::cout << "  Surface area:     " << net.total_surface_area_m2 << " m^2\n";
    std::cout << "  Total dP:         " << net.total_pressure_drop_Pa << " Pa  ("
              << units::Pa_to_kPa(net.total_pressure_drop_Pa) << " kPa / "
              << units::Pa_to_psi(net.total_pressure_drop_Pa) << " psi)\n";
    std::cout << "  Total Q_heat:     " << net.total_heat_transfer_W << " W  ("
              << units::W_to_kW(net.total_heat_transfer_W) << " kW / "
              << units::W_to_BTUhr(net.total_heat_transfer_W) << " BTU/hr)\n";
    std::cout << "  Pumping power:    " << net.total_pumping_power_W << " W\n";
    std::cout << "  COP (Q/W_pump):   " << net.COP_network << "\n";
    std::cout << "  Heat flux avg:    " << net.heat_flux_avg_Wm2 << " W/m^2\n";
    std::cout << "  T_inlet:          " << units::K_to_C(net.T_inlet_K) << " C\n";
    std::cout << "  T_outlet:         " << units::K_to_C(net.T_outlet_K) << " C\n";
    std::cout << "  dT_rise:          " << net.dT_rise_K << " K\n";
    std::cout << "  Effectiveness:    " << net.network_effectiveness << "\n";
    std::cout << "  Re mean/max/min:  " << net.Re_mean
              << " / " << net.Re_max << " / " << net.Re_min << "\n";
    std::cout << "  Nu mean/max:      " << net.Nu_mean << " / " << net.Nu_max << "\n";
    std::cout << "  h  mean/max:      " << net.h_mean << " / " << net.h_max << " W/(m^2 K)\n";
    std::cout << "  Eff mean:         " << net.eff_mean << "\n";
    std::cout << "  Spec. heat abs.:  " << net.specific_heat_kJkg << " kJ/kg\n";

    std::cout << "\n  Per-Segment:\n";
    std::cout << "  Seg  Regime        Re          f         dP(Pa)    Nu     h(W/m2K) T_in(C)  T_out(C) Q(W)      COP\n";
    std::cout << "  ---- ------------ ----------  --------  --------  -----  -------- -------  -------- --------  ------\n";
    for (const auto& s : net.segments) {
        printf("  %4u %-12s %10.0f  %8.6f  %8.1f  %5.2f  %8.1f %7.2f  %7.2f  %8.1f  %6.1f\n",
               s.segment_id, s.flow_regime.c_str(), s.Re, s.friction_factor,
               s.pressure_drop_Pa, s.Nu, s.h_conv,
               units::K_to_C(s.T_fluid_in_K),
               units::K_to_C(s.T_fluid_out_K),
               s.heat_transfer_W, s.COP_segment);
    }
    std::cout << "\n";
}

static void print_result_verbose(const NetworkResult& net) {
    print_result(net);

    // --- Thermal gradient ASCII bar ---
    const int BAR_W = 50;
    std::cout << "  Thermal Profile (T_fluid along pipe):\n";
    double T_wall_C = units::K_to_C(net.segments.empty() ? 373.15 : net.segments[0].T_wall_K);
    double T_min_C  = units::K_to_C(net.T_inlet_K);
    double T_range  = T_wall_C - T_min_C;
    if (T_range < 1e-3) T_range = 1.0;
    for (const auto& s : net.segments) {
        double frac = (units::K_to_C(s.T_fluid_out_K) - T_min_C) / T_range;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        int filled = static_cast<int>(frac * BAR_W);
        printf("  Seg%2u [%.*s%.*s] %6.2f C  Re=%6.0f (%s)\n",
               s.segment_id,
               filled, "##################################################",
               BAR_W - filled, "..................................................",
               units::K_to_C(s.T_fluid_out_K), s.Re,
               s.flow_regime.c_str());
    }

    // --- Dimensionless groups table ---
    std::cout << "\n  Dimensionless Groups:\n";
    std::cout << "  Seg  Pr       Pe       Gz       St(e-4)  Bi_wall  eps/D    Re*      \n";
    std::cout << "  ---- -------- -------- -------- -------- -------- -------- --------\n";
    for (const auto& s : net.segments) {
        printf("  %4u %8.3f %8.1f %8.2f %8.4f %8.5f %8.5f %8.2f\n",
               s.segment_id, s.Pr, s.Pe, s.Gz, s.St * 1e4, s.Bi_wall, s.eps_D, s.Re_star);
    }

    // --- Thermal resistance table ---
    std::cout << "\n  Thermal Resistance Breakdown:\n";
    std::cout << "  Seg  R_conv(K/W)  R_wall(K/W)  R_tot(K/W)  R_conv%  Q_flux(W/m^2)  LMTD(K)  eff\n";
    std::cout << "  ---- ----------- ------------ ----------- -------- -------------- -------- ------\n";
    for (const auto& s : net.segments) {
        double r_pct  = (s.R_total > 1e-30) ? 100.0 * s.R_conv / s.R_total : 0.0;
        double q_flux = (s.surface_area_m2 > 1e-30) ? s.heat_transfer_W / s.surface_area_m2 : 0.0;
        printf("  %4u %11.4f %12.4f %11.4f %7.1f%% %14.1f %8.2f %6.4f\n",
               s.segment_id, s.R_conv, s.R_wall, s.R_total, r_pct, q_flux, s.LMTD,
               s.thermal_efficiency);
    }

    // --- Flow rates ---
    std::cout << "\n  Volume & Mass Flow:\n";
    std::cout << "  Seg  v(m/s)  Q_v(m3/s)    Q_v(L/min)  Q_v(gpm)  m_dot(kg/s)  m_dot(kg/hr)\n";
    std::cout << "  ---- ------- ------------ ----------- --------- ------------ ------------\n";
    for (const auto& s : net.segments) {
        printf("  %4u %7.3f %12.6f %11.4f %9.4f %12.6f %12.4f\n",
               s.segment_id, s.velocity_ms,
               s.volume_flow_m3s, units::m3s_to_Lmin(s.volume_flow_m3s),
               units::m3s_to_gpm(s.volume_flow_m3s),
               s.mass_flow_kgs, s.mass_flow_kgs * 3600.0);
    }

    // --- Energy balance ---
    std::cout << "\n  Network Energy Balance:\n";
    printf("    Q_heat (fluid absorbed):     %10.3f W  (%7.4f kW)\n",
           net.total_heat_transfer_W, units::W_to_kW(net.total_heat_transfer_W));
    printf("    W_pump (mechanical input):   %10.3f W  (%7.4f kW)\n",
           net.total_pumping_power_W, units::W_to_kW(net.total_pumping_power_W));
    printf("    Net gain over pumping:       %10.3f W\n",
           net.total_heat_transfer_W - net.total_pumping_power_W);
    printf("    COP = Q_heat / W_pump:       %10.3f\n", net.COP_network);
    printf("    Specific enthalpy rise:      %10.4f kJ/kg\n", net.specific_heat_kJkg);
    printf("    Worst segment dP:            %10.2f Pa\n", net.dP_max_seg);
    printf("    Hottest segment Q:           %10.2f W\n", net.Q_max_seg);
    std::cout << "\n";
}

static void print_menu() {
    std::cout << "--- Commands ---\n";
    std::cout << "  [r]      Run simulation (compact output)\n";
    std::cout << "  [rv]     Run simulation (full verbose: dimensionless groups, resistance, flows, energy balance)\n";
    std::cout << "  [diag]   Full diagnostic block (regime + resistance tables)\n";
    std::cout << "  [c]      Show/edit configuration\n";
    std::cout << "  [f]      Select fluid (1=Water20, 2=Water80, 3=Air, 4=Glycol, 5=Oil)\n";
    std::cout << "  [m]      Select material (1=Cu, 2=Steel, 3=PVC, 4=SS, 5=Al)\n";
    std::cout << "  [n]      Set segment count\n";
    std::cout << "  [l]      Set segment length (m)\n";
    std::cout << "  [d]      Set inner diameter (mm)\n";
    std::cout << "  [v]      Set inlet velocity (m/s)\n";
    std::cout << "  [t]      Set T_inlet (C)\n";
    std::cout << "  [w]      Set T_wall (C)\n";
    std::cout << "  [s]      Toggle stochastic mode\n";
    std::cout << "  [sv]     Sweep velocity (parametric)\n";
    std::cout << "  [sd]     Sweep diameter (parametric)\n";
    std::cout << "  [st]     Sweep wall temperature (parametric)\n";
    std::cout << "  [sn]     Sweep segment count (parametric)\n";
    std::cout << "  [report] Export Markdown report (5-section, full detail)\n";
    std::cout << "  [csv]    Export CSV data (all 36 computed columns)\n";
    std::cout << "  [q]      Quit\n";
    std::cout << "\n> ";
}

static double read_double(const std::string& prompt, double current) {
    std::cout << prompt << " [" << current << "]: ";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return current;
    try { return std::stod(line); }
    catch (...) { return current; }
}

static int read_int(const std::string& prompt, int current) {
    std::cout << prompt << " [" << current << "]: ";
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return current;
    try { return std::stoi(line); }
    catch (...) { return current; }
}

int main(int argc, char* argv[]) {
    print_header();

    PipeThermalConfig cfg;

    // Parse optional CLI args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--batch" && i + 1 < argc) {
            // Batch mode: run with defaults and export
            cfg.n_segments = static_cast<uint32_t>(std::atoi(argv[++i]));
            auto net = solve_series_network(cfg);
            std::cout << generate_report(net, cfg);
            return 0;
        }
        if (arg == "--velocity" && i + 1 < argc) {
            cfg.inlet_velocity_ms = std::atof(argv[++i]);
        }
        if (arg == "--segments" && i + 1 < argc) {
            cfg.n_segments = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        if (arg == "--seed" && i + 1 < argc) {
            cfg.seed = static_cast<uint64_t>(std::atoll(argv[++i]));
        }
    }

    // Interactive REPL
    std::string cmd;
    while (true) {
        print_menu();
        if (!std::getline(std::cin, cmd)) break;

        if (cmd == "q" || cmd == "quit" || cmd == "exit") break;

        if (cmd == "r" || cmd == "run") {
            NetworkResult net;
            if (cfg.use_network) {
                net = solve_stochastic_network(cfg);
            } else {
                net = solve_series_network(cfg);
            }
            print_result(net);
        }
        else if (cmd == "rv" || cmd == "verbose") {
            NetworkResult net;
            if (cfg.use_network) net = solve_stochastic_network(cfg);
            else                 net = solve_series_network(cfg);
            print_result_verbose(net);
        }
        else if (cmd == "diag") {
            NetworkResult net;
            if (cfg.use_network) net = solve_stochastic_network(cfg);
            else                 net = solve_series_network(cfg);
            std::cout << net.diagnostic_block();
        }
        else if (cmd == "c" || cmd == "config") {
            print_config(cfg);
        }
        else if (cmd == "f") {
            int choice = read_int("Fluid (1=Water20,2=Water80,3=Air,4=Glycol,5=Oil)", 1);
            switch (choice) {
                case 1: cfg.fluid = water_20C(); break;
                case 2: cfg.fluid = water_80C(); break;
                case 3: cfg.fluid = air_20C(); break;
                case 4: cfg.fluid = ethylene_glycol(); break;
                case 5: cfg.fluid = engine_oil(); break;
                default: std::cout << "Invalid choice.\n"; break;
            }
            std::cout << "Fluid: " << cfg.fluid.name << "\n";
        }
        else if (cmd == "m") {
            int choice = read_int("Material (1=Cu,2=Steel,3=PVC,4=SS,5=Al)", 1);
            switch (choice) {
                case 1: cfg.material = copper_pipe(); break;
                case 2: cfg.material = steel_pipe(); break;
                case 3: cfg.material = pvc_pipe(); break;
                case 4: cfg.material = stainless_pipe(); break;
                case 5: cfg.material = aluminum_pipe(); break;
                default: std::cout << "Invalid choice.\n"; break;
            }
            std::cout << "Material: " << cfg.material.name << "\n";
        }
        else if (cmd == "n") {
            cfg.n_segments = static_cast<uint32_t>(read_int("Segments", cfg.n_segments));
        }
        else if (cmd == "l") {
            cfg.segment_length_m = read_double("Segment length (m)", cfg.segment_length_m);
        }
        else if (cmd == "d") {
            double d_mm = read_double("Inner diameter (mm)", units::m_to_mm(cfg.inner_diameter_m));
            double wall = (cfg.outer_diameter_m - cfg.inner_diameter_m) / 2.0;
            cfg.inner_diameter_m = d_mm / 1000.0;
            cfg.outer_diameter_m = cfg.inner_diameter_m + 2.0 * wall;
        }
        else if (cmd == "v") {
            cfg.inlet_velocity_ms = read_double("Inlet velocity (m/s)", cfg.inlet_velocity_ms);
        }
        else if (cmd == "t") {
            double t_c = read_double("T_inlet (C)", units::K_to_C(cfg.T_inlet_K));
            cfg.T_inlet_K = units::C_to_K(t_c);
        }
        else if (cmd == "w") {
            double t_c = read_double("T_wall (C)", units::K_to_C(cfg.T_wall_K));
            cfg.T_wall_K = units::C_to_K(t_c);
        }
        else if (cmd == "s") {
            cfg.use_network = !cfg.use_network;
            std::cout << "Stochastic mode: " << (cfg.use_network ? "ON" : "OFF") << "\n";
            if (cfg.use_network) {
                cfg.seed = static_cast<uint64_t>(read_int("Seed", static_cast<int>(cfg.seed)));
            }
        }
        else if (cmd == "sv") {
            double v_min = read_double("v_min (m/s)", 0.1);
            double v_max = read_double("v_max (m/s)", 5.0);
            int np = read_int("Points", 10);
            auto pts = sweep_velocity(cfg, v_min, v_max, np);
            std::cout << "\n  Velocity Sweep:\n";
            std::cout << "  v(m/s)   Re       Regime       dP(kPa)  Q_heat(W)  T_out(C)  Eff      COP\n";
            std::cout << "  -------  -------  -----------  -------  ---------  --------  -----    ------\n";
            for (auto& p : pts) {
                double re = p.result.Re_mean;
                const char* regime = re < 2300 ? "laminar" : re < 4000 ? "transitional" : "turbulent";
                printf("  %7.3f  %7.0f  %-12s %7.2f  %9.1f  %8.2f  %.4f   %.2f\n",
                       p.parameter_value, re, regime,
                       units::Pa_to_kPa(p.result.total_pressure_drop_Pa),
                       p.result.total_heat_transfer_W,
                       units::K_to_C(p.result.T_outlet_K),
                       p.result.network_effectiveness,
                       p.result.COP_network);
            }
        }
        else if (cmd == "sd") {
            double d_min = read_double("d_min (mm)", 10.0);
            double d_max = read_double("d_max (mm)", 50.0);
            int np = read_int("Points", 10);
            auto pts = sweep_diameter(cfg, d_min/1000.0, d_max/1000.0, np);
            std::cout << "\n  Diameter Sweep:\n";
            std::cout << "  D(mm)    Re       Regime       dP(kPa)  Q_heat(W)  T_out(C)  Q_flux(W/m2)  COP\n";
            std::cout << "  ------   -------  -----------  -------  ---------  --------  ------------  ------\n";
            for (auto& p : pts) {
                double re = p.result.Re_mean;
                const char* regime = re < 2300 ? "laminar" : re < 4000 ? "transitional" : "turbulent";
                printf("  %6.1f   %7.0f  %-12s %7.2f  %9.1f  %8.2f  %12.1f  %6.2f\n",
                       units::m_to_mm(p.parameter_value), re, regime,
                       units::Pa_to_kPa(p.result.total_pressure_drop_Pa),
                       p.result.total_heat_transfer_W,
                       units::K_to_C(p.result.T_outlet_K),
                       p.result.heat_flux_avg_Wm2,
                       p.result.COP_network);
            }
        }
        else if (cmd == "st") {
            double t_min = read_double("T_wall_min (C)", 50.0);
            double t_max = read_double("T_wall_max (C)", 200.0);
            int np = read_int("Points", 10);
            auto pts = sweep_wall_temperature(cfg, units::C_to_K(t_min), units::C_to_K(t_max), np);
            std::cout << "\n  Wall Temperature Sweep:\n";
            std::cout << "  T_wall(C)  dT_rise(K)  Q_heat(W)  T_out(C)  Eff      Q_flux(W/m2)  COP\n";
            std::cout << "  ---------  ----------  ---------  --------  -----    ------------  ------\n";
            for (auto& p : pts) {
                printf("  %9.1f  %10.2f  %9.1f  %8.2f  %.4f   %12.1f  %6.2f\n",
                       units::K_to_C(p.parameter_value),
                       p.result.dT_rise_K,
                       p.result.total_heat_transfer_W,
                       units::K_to_C(p.result.T_outlet_K),
                       p.result.network_effectiveness,
                       p.result.heat_flux_avg_Wm2,
                       p.result.COP_network);
            }
        }
        else if (cmd == "sn") {
            int n_min = read_int("n_min (segments)", 1);
            int n_max = read_int("n_max (segments)", 20);
            auto pts = sweep_segments(cfg, n_min, n_max);
            std::cout << "\n  Segment Count Sweep:\n";
            std::cout << "  N   L_tot(m)  dP(kPa)  Q_heat(W)  T_out(C)  Eff      Q_flux(W/m2)  COP\n";
            std::cout << "  --- --------- -------  ---------  --------  -----    ------------  ------\n";
            for (auto& p : pts) {
                printf("  %3d %9.2f %7.2f  %9.1f  %8.2f  %.4f   %12.1f  %6.2f\n",
                       static_cast<int>(p.parameter_value),
                       p.result.total_length_m,
                       units::Pa_to_kPa(p.result.total_pressure_drop_Pa),
                       p.result.total_heat_transfer_W,
                       units::K_to_C(p.result.T_outlet_K),
                       p.result.network_effectiveness,
                       p.result.heat_flux_avg_Wm2,
                       p.result.COP_network);
            }
        }
        else if (cmd == "report") {
            auto net = cfg.use_network ? solve_stochastic_network(cfg) : solve_series_network(cfg);
            std::string report = generate_report(net, cfg);
            std::string path = "output/pipe_thermal_report.md";
            std::ofstream ofs(path);
            if (ofs) {
                ofs << report;
                std::cout << "Report written to: " << path << "\n";
            } else {
                std::cout << report;
            }
        }
        else if (cmd == "csv") {
            auto net = cfg.use_network ? solve_stochastic_network(cfg) : solve_series_network(cfg);
            std::string csv = export_csv(net);
            std::string path = "output/pipe_thermal_data.csv";
            std::ofstream ofs(path);
            if (ofs) {
                ofs << csv;
                std::cout << "CSV written to: " << path << "\n";
            } else {
                std::cout << csv;
            }
        }
        else {
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }

    std::cout << "\nPipe thermal module session ended.\n";
    return 0;
}
