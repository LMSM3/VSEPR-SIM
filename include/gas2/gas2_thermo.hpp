/**
 * gas2_thermo.hpp
 * ---------------
 * Thermodynamic Potentials and Phase Equilibrium for gas2 Module.
 *
 * Implements the theoretical framework from:
 *   docs/section_gas2_thermopotentials.tex
 *
 * Provides:
 *   - Thermal de Broglie wavelength Lambda(T, M)
 *   - Helmholtz free energy A(T,V,N) with EOS departure functions
 *   - Gibbs free energy G(T,P,N) via Legendre transform
 *   - Entropy S(T,V,N) via Sackur-Tetrode + departure
 *   - Chemical potential mu(T,P) via fugacity coefficient
 *   - Fugacity coefficient phi from cubic EOS
 *   - Maxwell construction for saturation pressure
 *   - Phase boundary point (T_sat, P_sat, V_liq, V_vap)
 *
 * SI internally. Hartree conversion at render time only.
 * Anti-black-box: every intermediate is stored in result structs.
 */

#pragma once

#include "gas2_constants.hpp"
#include "gas2_species.hpp"
#include "gas2_eos.hpp"
#include "gas2_kinetic.hpp"
#include "gas2_heat.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

namespace vsepr {
namespace gas2 {

// ============================================================================
// Thermal de Broglie wavelength
// ============================================================================

// Lambda = sqrt(h^2 / (2*pi*m*kB*T))
// m in kg (per molecule), T in K, result in m
inline double thermal_wavelength(double T, double m_kg) {
    return std::sqrt(h_planck * h_planck / (TWO_PI * m_kg * kB * T));
}

// Convenience: from molar mass in g/mol
inline double thermal_wavelength_from_M(double T, double M_gmol) {
    double m_kg = M_gmol * 1e-3 / N_A;
    return thermal_wavelength(T, m_kg);
}

// ============================================================================
// Helmholtz free energy A(T,V,N)
// ============================================================================

struct HelmholtzResult {
    double A_ig;          // Ideal gas contribution (J)
    double A_dep;         // Departure from ideal (J)
    double A_total;       // A_ig + A_dep (J)
    double A_total_Eh;    // In Hartree
    double Lambda_m;      // Thermal wavelength (m)
    double T_K;
    double V_m3;
    double n_mol;
    std::string eos_type; // Which EOS was used for departure
};

// Ideal gas Helmholtz: A_ig = -NkT[ln(V/(N*Lambda^3)) + 1]
// Here N = n*N_A, but we work per mole: A_ig = -nRT[ln(V/(n*N_A*Lambda^3)) + 1]
inline double helmholtz_ideal(double n, double T, double V_m3, double Lambda_m) {
    double N = n * N_A;
    double ln_arg = V_m3 / (N * Lambda_m * Lambda_m * Lambda_m);
    return -N * kB * T * (std::log(ln_arg) + 1.0);
}

// VdW departure: A_dep = -nRT*ln((V - nb)/V) - n^2*a/V
// a in Pa*m^6/mol^2, b in m^3/mol
inline double helmholtz_departure_vdw(double n, double T, double V_m3,
                                       double a, double b) {
    double nb = n * b;
    if (V_m3 <= nb) return 0.0;  // unphysical region
    return -n * R_gas * T * std::log((V_m3 - nb) / V_m3)
           - n * n * a / V_m3;
}

// RK departure: A_dep = -nRT*ln((V-nb)/V) - (na/(b*sqrt(T)))*ln((V+nb)/V)
inline double helmholtz_departure_rk(double n, double T, double V_m3,
                                      double a_rk, double b_rk) {
    double nb = n * b_rk;
    if (V_m3 <= nb) return 0.0;
    return -n * R_gas * T * std::log((V_m3 - nb) / V_m3)
           - (n * a_rk / (b_rk * std::sqrt(T)))
             * std::log((V_m3 + nb) / V_m3);
}

// Full Helmholtz calculation
inline HelmholtzResult helmholtz(double n, double T, double V_m3,
                                  double M_gmol,
                                  double a_vdw, double b_vdw,
                                  EOSType eos = EOSType::VanDerWaals) {
    HelmholtzResult r{};
    r.T_K = T;
    r.V_m3 = V_m3;
    r.n_mol = n;

    double m_kg = M_gmol * 1e-3 / N_A;
    r.Lambda_m = thermal_wavelength(T, m_kg);
    r.A_ig = helmholtz_ideal(n, T, V_m3, r.Lambda_m);

    switch (eos) {
        case EOSType::VanDerWaals:
            r.A_dep = helmholtz_departure_vdw(n, T, V_m3, a_vdw, b_vdw);
            r.eos_type = "VanDerWaals";
            break;
        case EOSType::RedlichKwong:
            r.A_dep = helmholtz_departure_rk(n, T, V_m3, a_vdw, b_vdw);
            r.eos_type = "RedlichKwong";
            break;
        default:
            r.A_dep = 0.0;
            r.eos_type = "Ideal";
            break;
    }

    r.A_total = r.A_ig + r.A_dep;
    r.A_total_Eh = r.A_total / Hartree_J;
    return r;
}

// ============================================================================
// Gibbs free energy G(T,P,N) = A(T,V,N) + PV
// ============================================================================

struct GibbsResult {
    double G;           // J
    double G_Eh;        // Hartree
    double A;           // Helmholtz (J)
    double PV;          // Pressure-volume work (J)
    double V_m3;        // Volume found from EOS
    double Z;           // Compressibility factor
    double T_K;
    double P_Pa;
    double n_mol;
    std::string eos_type;
};

inline GibbsResult gibbs(double n, double T, double P_Pa,
                          const GasSpecies* sp,
                          EOSType eos = EOSType::VanDerWaals) {
    GibbsResult g{};
    g.T_K = T;
    g.P_Pa = P_Pa;
    g.n_mol = n;

    double V_m3;
    double Z_val;

    if (sp && eos == EOSType::VanDerWaals) {
        auto sol = vdw_solve_volume(n, T, P_Pa, sp->vdw_a, sp->vdw_b);
        V_m3 = sol.V_m3;
        Z_val = sol.Z;
        g.eos_type = "VanDerWaals";
    } else if (sp && eos == EOSType::RedlichKwong) {
        auto rk = rk_params_from_critical(sp->Tc_K, sp->Pc_atm * atm_to_Pa);
        auto sol = rk_solve_volume(n, T, P_Pa, rk.a_RK, rk.b_RK);
        V_m3 = sol.V_m3;
        Z_val = sol.Z;
        g.eos_type = "RedlichKwong";
    } else {
        auto sol = ideal_gas(n, T, P_Pa);
        V_m3 = sol.V_m3;
        Z_val = sol.Z;
        g.eos_type = "Ideal";
    }

    g.V_m3 = V_m3;
    g.Z = Z_val;
    g.PV = P_Pa * V_m3;

    double a_eos = sp ? sp->vdw_a : 0.0;
    double b_eos = sp ? sp->vdw_b : 0.0;
    auto h = helmholtz(n, T, V_m3, sp ? sp->molar_mass_g : 28.0,
                        a_eos, b_eos, eos);
    g.A = h.A_total;
    g.G = h.A_total + g.PV;
    g.G_Eh = g.G / Hartree_J;

    return g;
}

// ============================================================================
// Entropy S(T,V,N)
// ============================================================================

struct EntropyResult {
    double S_ig;          // Ideal gas entropy (J/K)
    double S_dep;         // Departure (J/K)
    double S_total;       // Total (J/K)
    double S_per_mol;     // Per mole (J/(mol·K))
};

// Sackur-Tetrode: S_ig = Nk[5/2 + ln(V/(N*Lambda^3))]
inline EntropyResult entropy_ideal(double n, double T, double V_m3,
                                    double M_gmol) {
    EntropyResult r{};
    double m_kg = M_gmol * 1e-3 / N_A;
    double Lambda = thermal_wavelength(T, m_kg);
    double N = n * N_A;
    double ln_arg = V_m3 / (N * Lambda * Lambda * Lambda);
    r.S_ig = N * kB * (2.5 + std::log(ln_arg));
    r.S_dep = 0.0;
    r.S_total = r.S_ig;
    r.S_per_mol = r.S_total / n;
    return r;
}

// ============================================================================
// Chemical potential mu(T,P) = G/N for pure substance
// ============================================================================

struct ChemicalPotentialResult {
    double mu_J;          // J per molecule
    double mu_Eh;         // Hartree per molecule
    double mu_eV;         // eV per molecule
    double mu_kJmol;      // kJ/mol
    double phi;           // Fugacity coefficient
    double f_Pa;          // Fugacity (Pa)
};

// Fugacity coefficient from VdW EOS:
// ln(phi) = b/(V-b) - ln(Z(1-b/V)) - 2a/(RTV)  (per mole)
inline double fugacity_coeff_vdw(double T, double V_m3_per_mol,
                                  double a, double b) {
    double Vm = V_m3_per_mol;
    double term1 = b / (Vm - b);
    double term2 = -std::log(Vm / (Vm - b));
    double term3 = -2.0 * a / (R_gas * T * Vm);
    // Z for VdW:
    double P_calc = R_gas * T / (Vm - b) - a / (Vm * Vm);
    double Z_calc = P_calc * Vm / (R_gas * T);
    double ln_phi = Z_calc - 1.0 + term1 + term2 + term3 - std::log(Z_calc);
    return std::exp(ln_phi);
}

inline ChemicalPotentialResult chemical_potential(
    double T, double P_Pa, const GasSpecies* sp,
    EOSType eos = EOSType::VanDerWaals) {

    ChemicalPotentialResult r{};
    auto g = gibbs(1.0, T, P_Pa, sp, eos);

    // mu = G/N for pure substance
    r.mu_J = g.G / N_A;
    r.mu_Eh = r.mu_J / Hartree_J;
    r.mu_eV = r.mu_J / eV_to_J;
    r.mu_kJmol = g.G * 1e-3;  // G is already per mol for n=1

    // Fugacity
    if (sp && eos == EOSType::VanDerWaals) {
        double Vm = g.V_m3;  // n=1, so V = Vm
        r.phi = fugacity_coeff_vdw(T, Vm, sp->vdw_a, sp->vdw_b);
    } else {
        r.phi = 1.0;  // Ideal gas
    }
    r.f_Pa = r.phi * P_Pa;

    return r;
}

// ============================================================================
// Maxwell construction — saturation pressure from equal fugacity
// ============================================================================

struct PhasePoint {
    double T_K;
    double P_sat_Pa;
    double V_liq_m3;      // Liquid molar volume
    double V_vap_m3;      // Vapour molar volume
    double Z_liq;
    double Z_vap;
    bool converged;
    int iterations;
};

// Evaluate VdW pressure at given V (per mole)
inline double vdw_pressure(double T, double Vm, double a, double b) {
    if (Vm <= b) return 1e12;  // repulsive wall
    return R_gas * T / (Vm - b) - a / (Vm * Vm);
}

// Find the three roots of VdW at given (T, P) by scanning
// Returns vector of molar volumes where P(V) ≈ P_target
inline std::vector<double> vdw_roots(double T, double P_target,
                                      double a, double b,
                                      int n_scan = 10000) {
    // Scan from b+eps to large V
    double V_min = b * 1.01;
    double V_max = 10.0 * R_gas * T / P_target;
    if (V_max < V_min * 10.0) V_max = V_min * 10.0;

    std::vector<double> roots;
    double dV = (V_max - V_min) / n_scan;
    double prev = vdw_pressure(T, V_min, a, b) - P_target;

    for (int i = 1; i <= n_scan; ++i) {
        double V = V_min + i * dV;
        double curr = vdw_pressure(T, V, a, b) - P_target;
        if (prev * curr < 0.0) {
            // Bisect to refine
            double lo = V - dV, hi = V;
            for (int j = 0; j < 60; ++j) {
                double mid = 0.5 * (lo + hi);
                double fm = vdw_pressure(T, mid, a, b) - P_target;
                if (fm * (vdw_pressure(T, lo, a, b) - P_target) < 0.0)
                    hi = mid;
                else
                    lo = mid;
            }
            roots.push_back(0.5 * (lo + hi));
        }
        prev = curr;
    }
    return roots;
}

// Maxwell construction: find P_sat where fugacity(liq) = fugacity(vap)
// Uses the equal-area rule on the P-V isotherm.
inline PhasePoint maxwell_construction(double T, double a, double b,
                                        double P_guess = 0.0,
                                        int max_iter = 200,
                                        double tol = 1.0) {
    PhasePoint pt{};
    pt.T_K = T;
    pt.converged = false;

    // Critical point for VdW: Tc = 8a/(27Rb), Pc = a/(27b^2)
    double Tc = 8.0 * a / (27.0 * R_gas * b);
    if (T >= Tc) {
        pt.converged = false;  // Supercritical — no phase boundary
        return pt;
    }

    double Pc = a / (27.0 * b * b);
    double P_lo = 0.01 * Pc;
    double P_hi = Pc * 0.999;
    if (P_guess > P_lo && P_guess < P_hi) {
        // Use as initial bracket centre
    }

    // Bisection on equal-area condition
    for (int iter = 0; iter < max_iter; ++iter) {
        double P_mid = 0.5 * (P_lo + P_hi);
        auto roots = vdw_roots(T, P_mid, a, b);

        if (roots.size() < 2) {
            // Only one root — above or at critical
            P_hi = P_mid;
            continue;
        }

        double V_liq = roots.front();
        double V_vap = roots.back();

        // Equal area: integral of (P(V) - P_sat) dV from V_liq to V_vap
        int n_quad = 1000;
        double dV = (V_vap - V_liq) / n_quad;
        double area = 0.0;
        for (int j = 0; j < n_quad; ++j) {
            double V = V_liq + (j + 0.5) * dV;
            area += (vdw_pressure(T, V, a, b) - P_mid) * dV;
        }

        if (std::abs(area) < tol) {
            pt.P_sat_Pa = P_mid;
            pt.V_liq_m3 = V_liq;
            pt.V_vap_m3 = V_vap;
            pt.Z_liq = P_mid * V_liq / (R_gas * T);
            pt.Z_vap = P_mid * V_vap / (R_gas * T);
            pt.converged = true;
            pt.iterations = iter + 1;
            return pt;
        }

        // If area > 0, P_sat is too low; if area < 0, too high
        if (area > 0.0)
            P_lo = P_mid;
        else
            P_hi = P_mid;
    }

    // Return best estimate
    double P_mid = 0.5 * (P_lo + P_hi);
    auto roots = vdw_roots(T, P_mid, a, b);
    pt.P_sat_Pa = P_mid;
    if (roots.size() >= 2) {
        pt.V_liq_m3 = roots.front();
        pt.V_vap_m3 = roots.back();
        pt.Z_liq = P_mid * roots.front() / (R_gas * T);
        pt.Z_vap = P_mid * roots.back() / (R_gas * T);
    }
    pt.iterations = max_iter;
    return pt;
}

// ============================================================================
// Phase envelope: multiple points along the coexistence curve
// ============================================================================

inline std::vector<PhasePoint> phase_envelope(
    double a, double b, int n_points = 50) {

    double Tc = 8.0 * a / (27.0 * R_gas * b);
    std::vector<PhasePoint> envelope;
    envelope.reserve(n_points);

    for (int i = 1; i <= n_points; ++i) {
        double Tr = 0.5 + 0.49 * static_cast<double>(i) / n_points;  // Tr from 0.5 to ~0.99
        double T = Tr * Tc;
        auto pt = maxwell_construction(T, a, b);
        if (pt.converged) {
            envelope.push_back(pt);
        }
    }
    return envelope;
}

} // namespace gas2
} // namespace vsepr
