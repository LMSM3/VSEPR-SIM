/**
 * gas2_potential.hpp
 * ------------------
 * Intermolecular Potential Decomposition and Free-Energy Functional
 * for gas2 Module.
 *
 * Implements the theoretical framework from:
 *   docs/section_gas2_thermopotentials.tex  (Sections 5 and 6)
 *
 * Provides:
 *   - 8-channel potential decomposition U_total = sum of terms
 *   - Landau-Ginzburg free-energy functional F[phi]
 *   - Density order parameter phi(r) = rho(r) - rho_c
 *   - Euler-Lagrange equilibrium solver (1D)
 *   - Snapshot struct for monitoring window
 *
 * Compatible with existing EnergyResult (src/pot/energy.hpp) but
 * extends it with polarization, many-body, and Hartree conversion.
 *
 * Anti-black-box: every channel stored separately, no hidden sums.
 */

#pragma once

#include "gas2_constants.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace vsepr {
namespace gas2 {

// ============================================================================
// 8-channel potential decomposition
// ============================================================================

struct PotentialDecomposition {
    double U_bond     = 0.0;   // Bond stretching       (J/mol)
    double U_angle    = 0.0;   // Angle bending         (J/mol)
    double U_torsion  = 0.0;   // Dihedral rotation     (J/mol)
    double U_vdw      = 0.0;   // Van der Waals / LJ    (J/mol)
    double U_coul     = 0.0;   // Coulomb electrostatics (J/mol)
    double U_pol      = 0.0;   // Polarization (SCF)    (J/mol)
    double U_many     = 0.0;   // Many-body corrections (J/mol)
    double U_total    = 0.0;   // Sum of all channels   (J/mol)

    // Per-channel counts (diagnostics)
    int n_bonds    = 0;
    int n_angles   = 0;
    int n_torsions = 0;
    int n_pairs    = 0;

    void recompute_total() {
        U_total = U_bond + U_angle + U_torsion + U_vdw
                + U_coul + U_pol + U_many;
    }

    // Convert a single channel to kJ/mol
    static double to_kJmol(double U_Jmol) { return U_Jmol * 1e-3; }

    // Convert a single channel to Hartree (per molecule)
    static double to_Eh(double U_Jmol) {
        return (U_Jmol / N_A) / Hartree_J;
    }

    // Array access for monitoring (8 channels)
    double channel(int i) const {
        switch (i) {
            case 0: return U_bond;
            case 1: return U_angle;
            case 2: return U_torsion;
            case 3: return U_vdw;
            case 4: return U_coul;
            case 5: return U_pol;
            case 6: return U_many;
            case 7: return U_total;
            default: return 0.0;
        }
    }

    static const char* channel_name(int i) {
        static const char* names[] = {
            "U_bond", "U_angle", "U_torsion", "U_vdw",
            "U_coul", "U_pol", "U_many", "U_total"
        };
        return (i >= 0 && i < 8) ? names[i] : "?";
    }

    static const char* channel_symbol(int i) {
        static const char* syms[] = {
            "bond", "angle", "tors", "vdW",
            "Coul", "pol", "many", "total"
        };
        return (i >= 0 && i < 8) ? syms[i] : "?";
    }

    std::string format_table() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4);
        ss << "  Channel       kJ/mol        Eh/molecule\n";
        ss << "  " << std::string(45, '-') << "\n";
        for (int i = 0; i < 8; ++i) {
            double v = channel(i);
            ss << "  " << std::setw(10) << std::left << channel_name(i)
               << std::setw(14) << std::right << to_kJmol(v)
               << std::setw(16) << std::scientific << std::setprecision(4)
               << to_Eh(v) << "\n" << std::fixed << std::setprecision(4);
        }
        return ss.str();
    }
};

// ============================================================================
// Landau-Ginzburg free-energy functional F[phi]
// ============================================================================

struct LandauParams {
    double a;        // Quadratic coefficient (changes sign at Tc)
    double b;        // Quartic coefficient (>0 for stability)
    double kappa;    // Gradient penalty (>0, interface cost)
    double T_K;      // Current temperature
    double Tc_K;     // Critical temperature

    // a(T) = a0 * (T - Tc), sign flip at Tc
    static LandauParams from_species(double T, double Tc,
                                      double sigma_m, double epsilon_J) {
        LandauParams p{};
        p.T_K = T;
        p.Tc_K = Tc;
        // a0 ~ kB / Vc, a(T) = a0(T - Tc)
        double a0 = kB / (sigma_m * sigma_m * sigma_m);
        p.a = a0 * (T - Tc);
        // b ~ kBTc / Vc^2
        double Vc = sigma_m * sigma_m * sigma_m;
        p.b = kB * Tc / (Vc * Vc) * 0.1;  // Scaled for numerical stability
        // kappa ~ sigma^2 * epsilon
        p.kappa = sigma_m * sigma_m * epsilon_J;
        return p;
    }
};

// 1D density profile phi(x) on a grid
struct DensityProfile {
    std::vector<double> phi;   // Order parameter values
    double dx;                 // Grid spacing (m)
    double L;                  // Domain length (m)

    // Initialise with a tanh interface profile
    void init_tanh_interface(int n_points, double L_m,
                              double phi_eq, double width) {
        L = L_m;
        dx = L_m / n_points;
        phi.resize(n_points);
        double x_mid = 0.5 * L_m;
        for (int i = 0; i < n_points; ++i) {
            double x = (i + 0.5) * dx;
            phi[i] = phi_eq * std::tanh((x - x_mid) / width);
        }
    }

    // Initialise with uniform density fluctuation
    void init_uniform(int n_points, double L_m, double phi0 = 0.0) {
        L = L_m;
        dx = L_m / n_points;
        phi.assign(n_points, phi0);
    }
};

// Evaluate F[phi] on a 1D profile
struct FreeEnergyResult {
    double F_bulk;      // Integral of a*phi^2 + b*phi^4 (J)
    double F_gradient;  // Integral of kappa*(dphi/dx)^2 (J)
    double F_total;     // F_bulk + F_gradient (J)
    double F_total_Eh;  // In Hartree

    // Per-point contributions for monitoring
    std::vector<double> f_density;   // f(x) = a*phi^2 + b*phi^4
    std::vector<double> f_gradient;  // g(x) = kappa*(dphi/dx)^2
};

inline FreeEnergyResult evaluate_free_energy(
    const DensityProfile& prof, const LandauParams& p) {

    FreeEnergyResult r{};
    int n = static_cast<int>(prof.phi.size());
    r.f_density.resize(n, 0.0);
    r.f_gradient.resize(n, 0.0);
    r.F_bulk = 0.0;
    r.F_gradient = 0.0;

    for (int i = 0; i < n; ++i) {
        double phi2 = prof.phi[i] * prof.phi[i];
        double phi4 = phi2 * phi2;
        r.f_density[i] = p.a * phi2 + p.b * phi4;
        r.F_bulk += r.f_density[i] * prof.dx;

        // Central difference for gradient
        double dphi_dx = 0.0;
        if (i > 0 && i < n - 1) {
            dphi_dx = (prof.phi[i + 1] - prof.phi[i - 1]) / (2.0 * prof.dx);
        } else if (i == 0 && n > 1) {
            dphi_dx = (prof.phi[1] - prof.phi[0]) / prof.dx;
        } else if (i == n - 1 && n > 1) {
            dphi_dx = (prof.phi[n - 1] - prof.phi[n - 2]) / prof.dx;
        }
        r.f_gradient[i] = p.kappa * dphi_dx * dphi_dx;
        r.F_gradient += r.f_gradient[i] * prof.dx;
    }

    r.F_total = r.F_bulk + r.F_gradient;
    r.F_total_Eh = r.F_total / Hartree_J;
    return r;
}

// ============================================================================
// Monitoring snapshot (consumed by the Tkinter/terminal panel)
// ============================================================================

struct MonitorSnapshot {
    PotentialDecomposition U;
    FreeEnergyResult F;
    LandauParams landau;
    DensityProfile profile;
    double T_K;
    std::string formula;
    int cycle;                  // Evaluation cycle counter

    // JSON for IPC to Python monitor
    std::string to_json() const {
        std::ostringstream ss;
        ss << std::scientific << std::setprecision(6);
        ss << "{";
        ss << "\"cycle\":" << cycle << ",";
        ss << "\"T_K\":" << T_K << ",";
        ss << "\"formula\":\"" << formula << "\",";

        // 8 U channels
        ss << "\"U\":[";
        for (int i = 0; i < 8; ++i) {
            if (i > 0) ss << ",";
            ss << U.channel(i);
        }
        ss << "],";

        // F functional
        ss << "\"F_bulk\":" << F.F_bulk << ",";
        ss << "\"F_gradient\":" << F.F_gradient << ",";
        ss << "\"F_total\":" << F.F_total << ",";
        ss << "\"F_total_Eh\":" << F.F_total_Eh << ",";

        // Landau params
        ss << "\"a\":" << landau.a << ",";
        ss << "\"b\":" << landau.b << ",";
        ss << "\"kappa\":" << landau.kappa << ",";

        // Density profile (truncated for IPC)
        int n = std::min(static_cast<int>(profile.phi.size()), 200);
        ss << "\"phi\":[";
        for (int i = 0; i < n; ++i) {
            if (i > 0) ss << ",";
            ss << profile.phi[i];
        }
        ss << "]";

        ss << "}";
        return ss.str();
    }
};

} // namespace gas2
} // namespace vsepr
