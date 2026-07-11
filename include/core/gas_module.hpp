/**
 * gas_module.hpp
 * --------------
 * Dedicated Gas-Phase Simulation and Analysis Module.
 *
 * Provides:
 *   - Ideal gas law (PV = nRT) calculations
 *   - Van der Waals equation of state
 *   - Maxwell-Boltzmann speed distribution sampling
 *   - Kinetic energy and RMS speed calculations
 *   - Mean free path estimation
 *   - Gas cloud generation with thermal velocity initialization
 *
 * All calculations are deterministic given seed.
 * Anti-black-box: every intermediate value is inspectable.
 *
 * Constants: CODATA 2018 values, consistent with energy_units.hpp.
 */

#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <array>
#include <sstream>
#include <iomanip>
#include <map>
#include <cstdint>

namespace vsepr {
namespace gas {

// ============================================================================
// Physical constants (CODATA 2018, SI)
// ============================================================================

constexpr double kB          = 1.380649e-23;    // J/K (exact, SI redefinition)
constexpr double R_gas       = 8.314462618;     // J/(mol·K)
constexpr double N_A         = 6.02214076e23;   // mol^-1 (exact)
constexpr double atm_to_Pa   = 101325.0;        // Pa/atm
constexpr double amu_to_kg   = 1.66053906660e-27; // kg/amu

// ============================================================================
// Van der Waals parameters (a in Pa·m^6/mol^2, b in m^3/mol)
// Common gases — from NIST
// ============================================================================

struct VdWParams {
    double a;   // Pa·m^6/mol^2
    double b;   // m^3/mol
};

inline const std::map<std::string, VdWParams>& vdw_database() {
    static const std::map<std::string, VdWParams> db = {
        {"H2",  {0.02476,  2.661e-5}},
        {"He",  {0.003457, 2.370e-5}},
        {"N2",  {0.1408,   3.913e-5}},
        {"O2",  {0.1378,   3.183e-5}},
        {"Ar",  {0.1355,   3.201e-5}},
        {"CO2", {0.3640,   4.267e-5}},
        {"H2O", {0.5536,   3.049e-5}},
        {"CH4", {0.2283,   4.278e-5}},
        {"Ne",  {0.02135,  1.709e-5}},
        {"Kr",  {0.2349,   3.978e-5}},
        {"Xe",  {0.4250,   5.105e-5}},
        {"NH3", {0.4225,   3.707e-5}},
        {"Cl2", {0.6579,   5.622e-5}},
        {"SO2", {0.6803,   5.636e-5}},
    };
    return db;
}

// Molar mass table (g/mol) for common gases
inline const std::map<std::string, double>& gas_molar_mass() {
    static const std::map<std::string, double> db = {
        {"H2",  2.016},
        {"He",  4.003},
        {"N2",  28.014},
        {"O2",  31.998},
        {"Ar",  39.948},
        {"CO2", 44.010},
        {"H2O", 18.015},
        {"CH4", 16.043},
        {"Ne",  20.180},
        {"Kr",  83.798},
        {"Xe",  131.293},
        {"NH3", 17.031},
        {"Cl2", 70.906},
        {"SO2", 64.066},
    };
    return db;
}

// ============================================================================
// Gas property result
// ============================================================================

struct GasProperties {
    std::string formula;
    double temperature_K;
    double pressure_Pa;
    double moles;
    double molar_mass_kg;       // kg/mol

    // Ideal gas
    double ideal_volume_m3;     // V = nRT/P

    // Van der Waals (if params available)
    bool   has_vdw;
    double vdw_volume_m3;       // numerical solution
    double compressibility_Z;   // PV/(nRT)

    // Kinetic theory
    double rms_speed_ms;        // sqrt(3kBT/m)
    double mean_speed_ms;       // sqrt(8kBT/(pi*m))
    double most_probable_speed_ms; // sqrt(2kBT/m)
    double avg_kinetic_energy_J;   // 3/2 kBT per molecule
    double mean_free_path_m;       // lambda = kBT/(sqrt(2)*pi*d^2*P)

    // Formatting
    std::string format_report() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4);
        ss << "╔════════════════════════════════════════════════════════════════╗\n";
        ss << "║  Gas Properties: " << std::setw(45) << std::left << formula << "║\n";
        ss << "╠════════════════════════════════════════════════════════════════╣\n";
        ss << "║  Temperature:          " << std::setw(12) << temperature_K << " K" << std::string(25, ' ') << "║\n";
        ss << "║  Pressure:             " << std::setw(12) << (pressure_Pa / atm_to_Pa) << " atm" << std::string(23, ' ') << "║\n";
        ss << "║  Moles:                " << std::setw(12) << moles << " mol" << std::string(23, ' ') << "║\n";
        ss << "║  Molar mass:           " << std::setw(12) << (molar_mass_kg * 1000.0) << " g/mol" << std::string(21, ' ') << "║\n";
        ss << "╠════════════════════════════════════════════════════════════════╣\n";
        ss << "║  Ideal Volume:         " << std::setw(12) << (ideal_volume_m3 * 1000.0) << " L" << std::string(25, ' ') << "║\n";
        if (has_vdw) {
            ss << "║  VdW Volume:           " << std::setw(12) << (vdw_volume_m3 * 1000.0) << " L" << std::string(25, ' ') << "║\n";
            ss << "║  Compressibility Z:    " << std::setw(12) << compressibility_Z << std::string(27, ' ') << "║\n";
        }
        ss << "╠════════════════════════════════════════════════════════════════╣\n";
        ss << "║  RMS speed:            " << std::setw(12) << rms_speed_ms << " m/s" << std::string(23, ' ') << "║\n";
        ss << "║  Mean speed:           " << std::setw(12) << mean_speed_ms << " m/s" << std::string(23, ' ') << "║\n";
        ss << "║  Most probable speed:  " << std::setw(12) << most_probable_speed_ms << " m/s" << std::string(23, ' ') << "║\n";
        ss << "║  Avg KE per molecule:  " << std::setw(12) << std::setprecision(6) << (avg_kinetic_energy_J / 1.602176634e-19) << " eV" << std::string(24, ' ') << "║\n";
        ss << "║  Mean free path:       " << std::setw(12) << std::setprecision(4) << (mean_free_path_m * 1e9) << " nm" << std::string(24, ' ') << "║\n";
        ss << "╚════════════════════════════════════════════════════════════════╝\n";
        return ss.str();
    }
};

// ============================================================================
// Velocity sample (for Maxwell-Boltzmann)
// ============================================================================

struct VelocitySample {
    double vx, vy, vz;
    double speed() const { return std::sqrt(vx*vx + vy*vy + vz*vz); }
};

// ============================================================================
// Core gas functions
// ============================================================================

// Ideal gas volume: V = nRT/P
inline double ideal_gas_volume(double n_mol, double T_K, double P_Pa) {
    return n_mol * R_gas * T_K / P_Pa;
}

// Ideal gas pressure: P = nRT/V
inline double ideal_gas_pressure(double n_mol, double T_K, double V_m3) {
    return n_mol * R_gas * T_K / V_m3;
}

// RMS speed: v_rms = sqrt(3RT/M) where M is molar mass in kg/mol
inline double rms_speed(double T_K, double M_kg_per_mol) {
    return std::sqrt(3.0 * R_gas * T_K / M_kg_per_mol);
}

// Mean speed: v_mean = sqrt(8RT/(pi*M))
inline double mean_speed(double T_K, double M_kg_per_mol) {
    return std::sqrt(8.0 * R_gas * T_K / (M_PI * M_kg_per_mol));
}

// Most probable speed: v_mp = sqrt(2RT/M)
inline double most_probable_speed(double T_K, double M_kg_per_mol) {
    return std::sqrt(2.0 * R_gas * T_K / M_kg_per_mol);
}

// Average kinetic energy per molecule: KE = 3/2 kBT
inline double avg_kinetic_energy(double T_K) {
    return 1.5 * kB * T_K;
}

// Mean free path: lambda = kBT / (sqrt(2) * pi * d^2 * P)
// d = kinetic diameter (default ~3.5e-10 m for typical gases)
inline double mean_free_path(double T_K, double P_Pa, double d_m = 3.5e-10) {
    return kB * T_K / (std::sqrt(2.0) * M_PI * d_m * d_m * P_Pa);
}

// Van der Waals volume solver (Newton's method)
// (P + a*n^2/V^2)(V - n*b) = nRT
// => P*V^3 - (n*b*P + n*R*T)*V^2 + a*n^2*V - a*n^3*b = 0
inline double vdw_volume(double n_mol, double T_K, double P_Pa,
                         double a, double b, int max_iter = 50) {
    // Initial guess from ideal gas
    double V = n_mol * R_gas * T_K / P_Pa;

    for (int i = 0; i < max_iter; ++i) {
        double V2 = V * V;
        double V3 = V2 * V;
        double nb = n_mol * b;
        double n2a = n_mol * n_mol * a;

        // f(V) = P*V^3 - (nb*P + nRT)*V^2 + n2a*V - n2a*nb
        double nRT = n_mol * R_gas * T_K;
        double f = P_Pa * V3 - (nb * P_Pa + nRT) * V2 + n2a * V - n2a * nb;
        double fp = 3.0 * P_Pa * V2 - 2.0 * (nb * P_Pa + nRT) * V + n2a;

        if (std::abs(fp) < 1e-30) break;
        double dV = f / fp;
        V -= dV;
        if (V < nb * 1.01) V = nb * 1.01; // physical lower bound
        if (std::abs(dV) < 1e-15 * V) break;
    }

    return V;
}

// Compute full gas properties
GasProperties compute_properties(const std::string& formula,
                                 double T_K, double P_atm, double n_mol = 1.0);

// Sample Maxwell-Boltzmann velocities
std::vector<VelocitySample> sample_maxwell_boltzmann(
    double T_K, double M_kg_per_mol, size_t count, uint64_t seed = 42);

// Format a speed distribution histogram (ASCII)
std::string format_speed_histogram(const std::vector<VelocitySample>& samples,
                                   int bins = 30, int width = 50);

// CLI dispatch: vsepr gas <subcommand> [args...]
int gas_dispatch(int argc, char** argv);

} // namespace gas
} // namespace vsepr
