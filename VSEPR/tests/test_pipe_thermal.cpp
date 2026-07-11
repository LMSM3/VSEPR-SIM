/**
 * test_pipe_thermal.cpp - Validation tests for Pipe Flow & Thermal Module
 *
 * Validates:
 *   1. Hagen-Poiseuille (laminar) pressure drop against analytical solution
 *   2. Reynolds number computation
 *   3. Friction factor (laminar: f = 64/Re)
 *   4. Nusselt number for laminar (3.66) and turbulent (Dittus-Boelter)
 *   5. Heat transfer energy balance
 *   6. Unit conversions (display-end only)
 *   7. Deterministic reproducibility (same seed -> same output)
 *   8. Welford heat capacity validation (prerequisite gap fix)
 *   9. Network result consistency
 */

#include "include/pipe_thermal/pipe_thermal_engine.hpp"
#include "atomistic/core/thermodynamics.hpp"
#include "atomistic/core/statistics.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>
#include <random>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name, const char* detail = "") {
    if (cond) {
        std::cout << "  [PASS] " << name;
        g_pass++;
    } else {
        std::cout << "  [FAIL] " << name;
        g_fail++;
    }
    if (detail[0]) std::cout << "  (" << detail << ")";
    std::cout << "\n";
}

static void check_near(double a, double b, double tol, const char* name) {
    double err = std::abs(a - b);
    char buf[256];
    snprintf(buf, sizeof(buf), "got=%.6g expected=%.6g err=%.6g tol=%.6g", a, b, err, tol);
    check(err < tol, name, buf);
}

int main() {
    using namespace pipe_thermal;
    std::cout << "\n=== Pipe Flow & Thermal Module Validation ===\n\n";

    // ========================================================================
    // Test 1: Laminar flow - Hagen-Poiseuille
    // ========================================================================
    std::cout << "--- Test 1: Laminar Hagen-Poiseuille ---\n";
    {
        // Analytical: dP = 128 * mu * L * Q / (pi * D^4)
        // Or equivalently: dP = f * (L/D) * (rho*v^2/2) with f = 64/Re
        FluidProperties fluid = water_20C();
        PipeMaterial mat = copper_pipe();
        double L = 1.0, D = 0.01, v = 0.01;  // very slow flow -> laminar

        auto seg = solve_segment(L, D, D + 0.004, v, 293.15, 373.15, fluid, mat);

        double Re_expected = fluid.density * v * D / fluid.viscosity;
        check_near(seg.Re, Re_expected, 0.01, "Reynolds number");
        check(seg.is_laminar, "Flow is laminar");

        double f_expected = 64.0 / Re_expected;
        check_near(seg.friction_factor, f_expected, 1e-4, "Friction factor f=64/Re");

        double dP_analytical = f_expected * (L/D) * (fluid.density * v * v / 2.0);
        check_near(seg.pressure_drop_Pa, dP_analytical, 0.01, "Pressure drop (analytical)");

        check_near(seg.Nu, 3.66, 0.01, "Nusselt number (laminar, const Tw)");
    }

    // ========================================================================
    // Test 2: Turbulent flow - Dittus-Boelter
    // ========================================================================
    std::cout << "\n--- Test 2: Turbulent Dittus-Boelter ---\n";
    {
        FluidProperties fluid = water_20C();
        PipeMaterial mat = copper_pipe();
        double L = 1.0, D = 0.025, v = 2.0;  // high velocity -> turbulent

        auto seg = solve_segment(L, D, D + 0.005, v, 293.15, 373.15, fluid, mat);

        double Re = fluid.density * v * D / fluid.viscosity;
        check(Re > 2300, "Reynolds > 2300 (turbulent)");
        check(!seg.is_laminar, "Flow regime detected as turbulent");

        double Pr = fluid.prandtl();
        double Nu_expected = 0.023 * std::pow(Re, 0.8) * std::pow(Pr, 0.4);
        check_near(seg.Nu, Nu_expected, 0.1, "Nusselt (Dittus-Boelter)");

        double h_expected = Nu_expected * fluid.conductivity / D;
        check_near(seg.h_conv, h_expected, 1.0, "Convection coefficient h");
    }

    // ========================================================================
    // Test 3: Energy balance
    // ========================================================================
    std::cout << "\n--- Test 3: Energy Balance ---\n";
    {
        PipeThermalConfig cfg;
        cfg.n_segments = 10;
        cfg.inlet_velocity_ms = 1.0;
        cfg.T_inlet_K = 293.15;
        cfg.T_wall_K = 373.15;

        auto net = solve_series_network(cfg);

        // T_outlet should approach T_wall as segments increase
        check(net.T_outlet_K > cfg.T_inlet_K, "Fluid heats up");
        check(net.T_outlet_K <= cfg.T_wall_K + 0.01, "T_outlet <= T_wall");

        // Heat transfer should be positive (heating)
        check(net.total_heat_transfer_W > 0, "Positive heat transfer");

        // Effectiveness between 0 and 1
        check(net.network_effectiveness > 0 && net.network_effectiveness <= 1.0,
              "Effectiveness in [0,1]");

        // Check Q = m_dot * Cp * (T_out - T_in)
        if (!net.segments.empty()) {
            double m_dot = net.segments[0].mass_flow_kgs;
            double Cp = cfg.fluid.specific_heat;
            double Q_check = m_dot * Cp * (net.T_outlet_K - net.T_inlet_K);
            check_near(net.total_heat_transfer_W, Q_check, 0.1,
                       "Q = m_dot*Cp*dT consistency");
        }
    }

    // ========================================================================
    // Test 4: Unit Conversions
    // ========================================================================
    std::cout << "\n--- Test 4: Unit Conversions ---\n";
    {
        check_near(units::K_to_C(273.15), 0.0, 0.001, "K->C: 273.15K = 0C");
        check_near(units::C_to_K(100.0), 373.15, 0.001, "C->K: 100C = 373.15K");
        check_near(units::K_to_F(273.15), 32.0, 0.01, "K->F: 273.15K = 32F");
        check_near(units::Pa_to_bar(1e5), 1.0, 1e-6, "Pa->bar: 1e5 Pa = 1 bar");
        check_near(units::Pa_to_atm(101325.0), 1.0, 1e-6, "Pa->atm: 101325 Pa = 1 atm");
        check_near(units::m_to_mm(0.025), 25.0, 1e-10, "m->mm: 0.025m = 25mm");
        check_near(units::W_to_kW(1000.0), 1.0, 1e-10, "W->kW: 1000W = 1kW");
    }

    // ========================================================================
    // Test 5: Deterministic Reproducibility
    // ========================================================================
    std::cout << "\n--- Test 5: Deterministic Reproducibility ---\n";
    {
        PipeThermalConfig cfg;
        cfg.use_network = true;
        cfg.seed = 12345;
        cfg.n_segments = 8;

        auto r1 = solve_stochastic_network(cfg);
        auto r2 = solve_stochastic_network(cfg);

        check_near(r1.total_pressure_drop_Pa, r2.total_pressure_drop_Pa, 1e-10,
                   "dP deterministic");
        check_near(r1.total_heat_transfer_W, r2.total_heat_transfer_W, 1e-10,
                   "Q deterministic");
        check_near(r1.T_outlet_K, r2.T_outlet_K, 1e-10,
                   "T_out deterministic");

        // Different seed must give different results
        cfg.seed = 99999;
        auto r3 = solve_stochastic_network(cfg);
        check(std::abs(r1.T_outlet_K - r3.T_outlet_K) > 1e-6,
              "Different seed -> different result");
    }

    // ========================================================================
    // Test 6: Sweep Consistency
    // ========================================================================
    std::cout << "\n--- Test 6: Sweep Consistency ---\n";
    {
        PipeThermalConfig cfg;
        cfg.n_segments = 3;

        auto pts = sweep_velocity(cfg, 0.5, 3.0, 5);
        check(pts.size() == 5, "Sweep returns correct point count");

        // dP should increase with velocity
        bool dp_increasing = true;
        for (size_t i = 1; i < pts.size(); ++i) {
            if (pts[i].result.total_pressure_drop_Pa < pts[i-1].result.total_pressure_drop_Pa) {
                dp_increasing = false;
                break;
            }
        }
        check(dp_increasing, "dP increases with velocity");
    }

    // ========================================================================
    // Test 7: Welford Heat Capacity (Prerequisite Gap Fix)
    // ========================================================================
    std::cout << "\n--- Test 7: Welford Heat Capacity Validation ---\n";
    {
        // Generate synthetic energy trajectory from a harmonic oscillator:
        // E = 0.5*k*x^2, thermally distributed -> <E> = 0.5*kB*T per DOF
        // For 1 DOF at temperature T, Cv = kB (ideal gas)
        //
        // We use a known distribution: E ~ Gaussian(mean, var)
        // with var = kB * T^2 * Cv_target  =>  Cv = kB + var/(kB*T^2)

        double T = 300.0;  // K
        double kB = atomistic::thermo::kB;
        double Cv_target = 3.0 * kB;  // 3 DOF ideal gas

        // var_E = (Cv_target - kB) * kB * T^2
        double var_E = (Cv_target - kB) * kB * T * T;
        double mean_E = -50.0;  // arbitrary baseline

        std::mt19937 rng(42);
        std::normal_distribution<double> dist(mean_E, std::sqrt(var_E));

        // Test naive (existing) function
        std::vector<double> E_traj;
        atomistic::OnlineStats stats;
        for (int i = 0; i < 100000; ++i) {
            double E = dist(rng);
            E_traj.push_back(E);
            stats.add_sample(E);
        }

        double Cv_naive = atomistic::thermo::heat_capacity_from_fluctuations(E_traj, T);
        double Cv_welford = atomistic::thermo::heat_capacity_welford(stats, T);

        check_near(Cv_naive, Cv_target, 0.1 * Cv_target, "Naive Cv within 10%");
        check_near(Cv_welford, Cv_target, 0.1 * Cv_target, "Welford Cv within 10%");

        // Welford should match naive for this well-conditioned case
        check_near(Cv_welford, Cv_naive, 1e-6, "Welford == Naive (well-conditioned)");

        // Test edge cases
        atomistic::OnlineStats empty_stats;
        check_near(atomistic::thermo::heat_capacity_welford(empty_stats, T), 0.0, 1e-10,
                   "Welford returns 0 for empty data");
        check_near(atomistic::thermo::heat_capacity_welford(stats, 0.0), 0.0, 1e-10,
                   "Welford returns 0 for T=0");
    }

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "\n=== Summary: " << g_pass << " PASS, " << g_fail << " FAIL"
              << " (total " << (g_pass + g_fail) << ") ===\n\n";

    return g_fail > 0 ? 1 : 0;
}
