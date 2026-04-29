/**
 * gas2_eos.hpp
 * ------------
 * Equations of State for gas2 Module.
 *
 * Provides:
 *   - Ideal gas law
 *   - Van der Waals EOS
 *   - Redlich-Kwong EOS
 *   - Compressibility factor Z
 *   - Virial expansion (second virial coefficient)
 *
 * Mirrors pattern: pipe_thermal_engine.hpp (fluid EOS)
 *                  gas_module.hpp          (ideal + VdW)
 *
 * All solvers are deterministic (Newton iteration with fixed bounds).
 * Anti-black-box: intermediate Re, Z, correction terms are exposed.
 */

#pragma once

#include "gas2_constants.hpp"
#include "gas2_species.hpp"
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

namespace vsepr {
namespace gas2 {

// ============================================================================
// EOS type selector
// ============================================================================

enum class EOSType {
    Ideal,
    VanDerWaals,
    RedlichKwong
};

inline std::string eos_name(EOSType e) {
    switch (e) {
        case EOSType::Ideal:        return "Ideal Gas";
        case EOSType::VanDerWaals:  return "Van der Waals";
        case EOSType::RedlichKwong: return "Redlich-Kwong";
    }
    return "Unknown";
}

// ============================================================================
// EOS result (all intermediate values exposed)
// ============================================================================

struct EOSResult {
    EOSType  type;
    double   T_K;           // temperature
    double   P_Pa;          // pressure
    double   n_mol;         // amount
    double   V_m3;          // volume (solved or given)
    double   Z;             // compressibility factor PV/(nRT)
    double   rho_molm3;     // molar density n/V
    int      iterations;    // solver iteration count (0 for ideal)
    bool     converged;

    double V_L()       const { return V_m3 * 1000.0; }
    double rho_kgm3(double M_kg) const { return rho_molm3 * M_kg; }
};

// ============================================================================
// Ideal gas
// ============================================================================

inline EOSResult ideal_gas(double n, double T, double P_Pa) {
    double V = n * R_gas * T / P_Pa;
    return {EOSType::Ideal, T, P_Pa, n, V, 1.0, n / V, 0, true};
}

inline double ideal_pressure(double n, double T, double V) {
    return n * R_gas * T / V;
}

// ============================================================================
// Van der Waals: (P + a·n²/V²)(V − n·b) = nRT
// Newton solver for V given T, P, n, a, b
// ============================================================================

inline EOSResult vdw_solve_volume(double n, double T, double P_Pa,
                                  double a, double b, int max_iter = 60) {
    double nRT = n * R_gas * T;
    double V = nRT / P_Pa;  // ideal initial guess
    int iter = 0;
    bool ok = false;

    for (; iter < max_iter; ++iter) {
        double nb = n * b;
        double n2a = n * n * a;
        double V2 = V * V;
        double V3 = V2 * V;

        double f  = P_Pa * V3 - (nb * P_Pa + nRT) * V2 + n2a * V - n2a * nb;
        double fp = 3.0 * P_Pa * V2 - 2.0 * (nb * P_Pa + nRT) * V + n2a;

        if (std::abs(fp) < 1e-30) break;
        double dV = f / fp;
        V -= dV;
        if (V < nb * 1.001) V = nb * 1.001;
        if (std::abs(dV) < 1e-15 * std::abs(V)) { ok = true; break; }
    }

    double Z = P_Pa * V / (n * R_gas * T);
    return {EOSType::VanDerWaals, T, P_Pa, n, V, Z, n / V, iter, ok};
}

// ============================================================================
// Redlich-Kwong: P = RT/(Vm − b) − a / (T^0.5 · Vm · (Vm + b))
// where Vm = V/n, a_RK and b_RK from critical constants:
//   a_RK = 0.42748 · R² · Tc^2.5 / Pc
//   b_RK = 0.08664 · R · Tc / Pc
// ============================================================================

struct RKParams {
    double a_RK;    // Pa·m^6·K^0.5/mol^2
    double b_RK;    // m^3/mol
};

inline RKParams rk_params_from_critical(double Tc_K, double Pc_Pa) {
    RKParams p;
    p.a_RK = 0.42748 * R_gas * R_gas * std::pow(Tc_K, 2.5) / Pc_Pa;
    p.b_RK = 0.08664 * R_gas * Tc_K / Pc_Pa;
    return p;
}

inline EOSResult rk_solve_volume(double n, double T, double P_Pa,
                                 double a_RK, double b_RK, int max_iter = 60) {
    double Vm = R_gas * T / P_Pa;  // ideal molar volume guess
    int iter = 0;
    bool ok = false;
    double sqrtT = std::sqrt(T);

    for (; iter < max_iter; ++iter) {
        // f(Vm) = P - RT/(Vm-b) + a/(sqrtT·Vm·(Vm+b))
        double denom1 = Vm - b_RK;
        double denom2 = sqrtT * Vm * (Vm + b_RK);
        if (std::abs(denom1) < 1e-30 || std::abs(denom2) < 1e-30) break;

        double f = P_Pa - R_gas * T / denom1 + a_RK / denom2;

        // f'(Vm) = RT/(Vm-b)^2 - a·(2Vm+b)/(sqrtT·Vm^2·(Vm+b)^2)
        double fp = R_gas * T / (denom1 * denom1)
                    - a_RK * (2.0 * Vm + b_RK)
                      / (sqrtT * Vm * Vm * (Vm + b_RK) * (Vm + b_RK));

        if (std::abs(fp) < 1e-30) break;
        double dVm = f / fp;
        Vm -= dVm;
        if (Vm < b_RK * 1.001) Vm = b_RK * 1.001;
        if (std::abs(dVm) < 1e-15 * std::abs(Vm)) { ok = true; break; }
    }

    double V = n * Vm;
    double Z = P_Pa * Vm / (R_gas * T);
    return {EOSType::RedlichKwong, T, P_Pa, n, V, Z, n / V, iter, ok};
}

// ============================================================================
// Convenience: solve using species data, auto-selecting best EOS
// ============================================================================

inline EOSResult solve_eos(const GasSpecies& sp, double T, double P_Pa,
                           double n, EOSType type = EOSType::VanDerWaals) {
    switch (type) {
        case EOSType::Ideal:
            return ideal_gas(n, T, P_Pa);
        case EOSType::VanDerWaals:
            return vdw_solve_volume(n, T, P_Pa, sp.vdw_a, sp.vdw_b);
        case EOSType::RedlichKwong: {
            auto rk = rk_params_from_critical(sp.Tc_K, sp.Pc_atm * atm_to_Pa);
            return rk_solve_volume(n, T, P_Pa, rk.a_RK, rk.b_RK);
        }
    }
    return ideal_gas(n, T, P_Pa);
}

} // namespace gas2
} // namespace vsepr
