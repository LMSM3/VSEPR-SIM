#pragma once
/**
 * phase_model.hpp
 * ===============
 * Three-tier phase model for melt/gas behavior estimation.
 *
 * Mode A: ThresholdEnthalpy (fast)
 *   - Melting: enthalpy threshold H(T) >= H_melt,onset + dH_f
 *   - Gas:    ideal gas with compressibility Z-correction
 *   - Use:    screening, bulk sweeps, millions of cases
 *
 * Mode B: SmoothedPhase (practical)
 *   - Melting: smoothed liquid fraction f_l(T) via sigmoid or piecewise
 *   - Gas:    cubic EOS (VdW or Redlich-Kwong via gas2_eos.hpp)
 *   - Use:    transition-region modeling, standard simulation
 *
 * Mode C: StabilityCompetition (serious)
 *   - Melting: Gibbs free energy competition G_s vs G_l
 *   - Gas:    fugacity-based treatment
 *   - Use:    extreme/exotic materials, publication-grade
 *
 * Output variables (all models):
 *   melt_score           [0,1]     liquid fraction estimate
 *   gas_nonideality_score          |Z - 1|
 *   phase_risk_score     [0,1]     proximity to transition
 *   temperature_severity [0,1]     normalized thermal stress
 *
 * Anti-black-box: every intermediate (H, f_l, G_s, G_l, Z, fugacity)
 * is exposed in the result structs.
 *
 * Physical constants follow atomistic::thermo convention:
 *   kB = 0.001987204 kcal/(mol·K)
 *   R  = 8.314462618 J/(mol·K) (for EOS, SI units)
 */

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

namespace atomistic {
namespace phase {

// ============================================================================
// Physical constants (SI for phase modeling)
// ============================================================================

constexpr double R_gas_SI = 8.314462618;   // J/(mol·K)
constexpr double kB_SI    = 1.380649e-23;  // J/K

// ============================================================================
// Phase model mode selector
// ============================================================================

enum class PhaseModelMode : uint8_t {
    A_THRESHOLD_ENTHALPY   = 0,  // Fast screening
    B_SMOOTHED_PHASE       = 1,  // Practical simulation
    C_STABILITY_COMPETITION= 2   // Publication-grade
};

inline const char* mode_name(PhaseModelMode m) {
    switch (m) {
        case PhaseModelMode::A_THRESHOLD_ENTHALPY:    return "Mode A: Threshold-Enthalpy";
        case PhaseModelMode::B_SMOOTHED_PHASE:        return "Mode B: Smoothed Phase-Fraction";
        case PhaseModelMode::C_STABILITY_COMPETITION: return "Mode C: Stability-Competition";
        default: return "Unknown";
    }
}

// ============================================================================
// Material phase parameters
// ============================================================================

struct PhaseParams {
    // Identity
    std::string name;
    double molar_mass_kg = 0.0;    // kg/mol

    // Melting
    double T_melt_K      = 0.0;    // Melting point (K)
    double T_solidus_K   = 0.0;    // Solidus (for range), defaults to T_melt
    double T_liquidus_K  = 0.0;    // Liquidus (for range), defaults to T_melt
    double dH_fusion_J   = 0.0;    // Latent heat of fusion (J/mol)

    // Heat capacities
    double cp_solid_J    = 25.0;   // J/(mol·K) solid heat capacity
    double cp_liquid_J   = 30.0;   // J/(mol·K) liquid heat capacity

    // Densities
    double rho_solid     = 0.0;    // kg/m^3
    double rho_liquid    = 0.0;    // kg/m^3

    // Boiling / gas
    double T_boil_K      = 0.0;    // Boiling point (K)
    double dH_vap_J      = 0.0;    // Latent heat of vaporization (J/mol)

    // Critical constants (for EOS)
    double Tc_K          = 0.0;    // Critical temperature (K)
    double Pc_Pa         = 0.0;    // Critical pressure (Pa)

    // VdW parameters (if known directly)
    double vdw_a         = 0.0;    // Pa·m^6/mol^2
    double vdw_b         = 0.0;    // m^3/mol

    // Free energy parameters (Mode C)
    double H_solid_ref   = 0.0;    // Reference enthalpy solid (J/mol)
    double S_solid_ref   = 0.0;    // Reference entropy solid (J/(mol·K))
    double V_solid_m3    = 0.0;    // Molar volume solid (m^3/mol)
    double H_liquid_ref  = 0.0;    // Reference enthalpy liquid (J/mol)
    double S_liquid_ref  = 0.0;    // Reference entropy liquid (J/(mol·K))
    double V_liquid_m3   = 0.0;    // Molar volume liquid (m^3/mol)

    // Mode B sigmoid steepness
    double sigmoid_k     = 0.05;   // K^-1 (steepness of transition)

    // Mode C Boltzmann softening
    double gamma_J       = 500.0;  // J/mol (softening scale for ΔG → f_l)

    void default_solidus_liquidus() {
        if (T_solidus_K <= 0.0) T_solidus_K = T_melt_K - 5.0;
        if (T_liquidus_K <= 0.0) T_liquidus_K = T_melt_K + 5.0;
    }
};

// ============================================================================
// Melt result — exposed intermediates
// ============================================================================

struct MeltResult {
    PhaseModelMode mode;
    double T_K                = 0.0;   // Input temperature
    double melt_score         = 0.0;   // [0,1] liquid fraction
    double H_accumulated_J    = 0.0;   // Accumulated enthalpy (Mode A)
    double H_onset_J          = 0.0;   // Onset enthalpy (Mode A)
    double f_liquid           = 0.0;   // Liquid fraction
    double cp_effective_J     = 0.0;   // Effective heat capacity
    double rho_effective      = 0.0;   // Effective density
    double G_solid_J          = 0.0;   // Gibbs free energy solid (Mode C)
    double G_liquid_J         = 0.0;   // Gibbs free energy liquid (Mode C)
    double dG_melt_J          = 0.0;   // ΔG_melt = G_l - G_s (Mode C)
    bool   is_melting         = false;
    bool   fully_liquid       = false;
};

// ============================================================================
// Gas result — exposed intermediates
// ============================================================================

struct GasResult {
    PhaseModelMode mode;
    double T_K             = 0.0;
    double P_Pa            = 0.0;
    double n_mol           = 0.0;
    double V_m3            = 0.0;
    double Z               = 1.0;   // Compressibility factor
    double gas_nonideality = 0.0;   // |Z - 1|
    double fugacity_Pa     = 0.0;   // Fugacity (Mode C)
    double phi             = 1.0;   // Fugacity coefficient f = phi*P
    bool   converged       = true;
    int    iterations      = 0;
};

// ============================================================================
// Combined phase report
// ============================================================================

struct PhaseReport {
    PhaseModelMode mode;
    double T_K                = 0.0;

    // The four output variables
    double melt_score         = 0.0;   // [0,1]
    double gas_nonideality_score = 0.0;// |Z - 1|
    double phase_risk_score   = 0.0;   // [0,1] proximity to transition
    double temperature_severity = 0.0; // [0,1] normalized thermal stress

    // Full sub-results
    MeltResult melt;
    GasResult  gas;

    std::string summary() const {
        std::ostringstream o;
        o << std::fixed << std::setprecision(4);
        o << "Phase Report [" << mode_name(mode) << "] @ T=" << T_K << " K\n";
        o << "  melt_score            = " << melt_score << "\n";
        o << "  gas_nonideality_score = " << gas_nonideality_score << "\n";
        o << "  phase_risk_score      = " << phase_risk_score << "\n";
        o << "  temperature_severity  = " << temperature_severity << "\n";
        if (mode == PhaseModelMode::C_STABILITY_COMPETITION) {
            o << "  G_solid  = " << melt.G_solid_J << " J/mol\n";
            o << "  G_liquid = " << melt.G_liquid_J << " J/mol\n";
            o << "  dG_melt  = " << melt.dG_melt_J << " J/mol\n";
            o << "  fugacity = " << gas.fugacity_Pa << " Pa\n";
        }
        return o.str();
    }
};

// ============================================================================
// Mode A: Threshold-Enthalpy Model (fast screening)
// ============================================================================

namespace mode_a {

/**
 * Enthalpy accumulation:
 *   H(T) ≈ ∫[T0→T] cp(T) dT
 *
 * Melt onset:  H(T) >= H_melt,onset
 * Full melt:   H(T) >= H_melt,onset + ΔH_f
 */
inline MeltResult threshold_melt(double T_K, const PhaseParams& p) {
    MeltResult r;
    r.mode = PhaseModelMode::A_THRESHOLD_ENTHALPY;
    r.T_K  = T_K;

    if (p.T_melt_K <= 0.0) {
        r.melt_score = 0.0;
        return r;
    }

    // Accumulated enthalpy from 0 K (simplified linear cp)
    r.H_accumulated_J = p.cp_solid_J * T_K;

    // Onset enthalpy at melting point
    r.H_onset_J = p.cp_solid_J * p.T_melt_K;

    if (r.H_accumulated_J < r.H_onset_J) {
        // Below melting onset
        r.melt_score = 0.0;
        r.f_liquid   = 0.0;
        r.cp_effective_J = p.cp_solid_J;
    }
    else if (r.H_accumulated_J < r.H_onset_J + p.dH_fusion_J) {
        // In melting range
        double progress = (p.dH_fusion_J > 0.0)
            ? (r.H_accumulated_J - r.H_onset_J) / p.dH_fusion_J
            : 1.0;
        r.melt_score  = std::clamp(progress, 0.0, 1.0);
        r.f_liquid    = r.melt_score;
        r.is_melting  = true;
        r.cp_effective_J = (1.0 - r.f_liquid) * p.cp_solid_J
                         + r.f_liquid * p.cp_liquid_J;
    }
    else {
        // Fully liquid
        r.melt_score   = 1.0;
        r.f_liquid     = 1.0;
        r.fully_liquid = true;
        r.cp_effective_J = p.cp_liquid_J;
    }

    // Effective density (linear mixing)
    if (p.rho_solid > 0.0 && p.rho_liquid > 0.0) {
        r.rho_effective = (1.0 - r.f_liquid) * p.rho_solid
                        + r.f_liquid * p.rho_liquid;
    }

    return r;
}

/**
 * Gas behavior: ideal gas with Z-correction.
 *   PV = ZnRT
 *   Z_est = 1 + α·(P/T) + β·ρ
 *
 * For fast screening, use small empirical corrections.
 */
inline GasResult ideal_gas_z(double T_K, double P_Pa, double n_mol,
                             double alpha = 0.0, double beta = 0.0,
                             double rho = 0.0) {
    GasResult r;
    r.mode  = PhaseModelMode::A_THRESHOLD_ENTHALPY;
    r.T_K   = T_K;
    r.P_Pa  = P_Pa;
    r.n_mol = n_mol;

    // Base Z
    r.Z = 1.0;
    if (T_K > 0.0)
        r.Z += alpha * (P_Pa / T_K);
    r.Z += beta * rho;

    // Volume from PV = ZnRT
    if (P_Pa > 0.0)
        r.V_m3 = r.Z * n_mol * R_gas_SI * T_K / P_Pa;

    r.gas_nonideality = std::abs(r.Z - 1.0);
    r.fugacity_Pa     = P_Pa;  // Ideal: fugacity = pressure
    r.converged       = true;
    return r;
}

} // namespace mode_a

// ============================================================================
// Mode B: Smoothed Phase-Fraction Model (practical)
// ============================================================================

namespace mode_b {

/**
 * Sigmoid liquid fraction:
 *   f_l(T) = 1 / (1 + exp[-k·(T - T_m)])
 */
inline double sigmoid_liquid_fraction(double T_K, double T_melt_K, double k) {
    double x = -k * (T_K - T_melt_K);
    // Guard against overflow
    if (x > 500.0) return 0.0;
    if (x < -500.0) return 1.0;
    return 1.0 / (1.0 + std::exp(x));
}

/**
 * Piecewise linear liquid fraction:
 *   f_l(T) = 0              if T < T_solidus
 *          = (T-Ts)/(Tl-Ts) if Ts <= T <= Tl
 *          = 1              if T > T_liquidus
 */
inline double piecewise_liquid_fraction(double T_K, double T_solidus, double T_liquidus) {
    if (T_K < T_solidus)  return 0.0;
    if (T_K > T_liquidus) return 1.0;
    double dT = T_liquidus - T_solidus;
    if (dT <= 0.0) return (T_K >= T_solidus) ? 1.0 : 0.0;
    return (T_K - T_solidus) / dT;
}

/**
 * Derivative of sigmoid: df_l/dT = k · f_l · (1 - f_l)
 */
inline double sigmoid_derivative(double f_l, double k) {
    return k * f_l * (1.0 - f_l);
}

/**
 * Effective heat capacity with latent heat contribution:
 *   cp_eff = (1-f_l)·cp_s + f_l·cp_l + ΔH_f · df_l/dT
 */
inline double effective_cp(double f_l, double df_dT,
                           double cp_solid, double cp_liquid, double dH_fusion) {
    return (1.0 - f_l) * cp_solid + f_l * cp_liquid + dH_fusion * df_dT;
}

/**
 * Smoothed melt result using sigmoid model.
 */
inline MeltResult smoothed_melt(double T_K, const PhaseParams& p, bool use_piecewise = false) {
    MeltResult r;
    r.mode = PhaseModelMode::B_SMOOTHED_PHASE;
    r.T_K  = T_K;

    if (p.T_melt_K <= 0.0) {
        r.melt_score = 0.0;
        r.cp_effective_J = p.cp_solid_J;
        return r;
    }

    // Compute liquid fraction
    if (use_piecewise) {
        double Ts = (p.T_solidus_K > 0.0) ? p.T_solidus_K : (p.T_melt_K - 5.0);
        double Tl = (p.T_liquidus_K > 0.0) ? p.T_liquidus_K : (p.T_melt_K + 5.0);
        r.f_liquid = piecewise_liquid_fraction(T_K, Ts, Tl);

        // Piecewise derivative
        double dT = Tl - Ts;
        double df_dT = (T_K >= Ts && T_K <= Tl && dT > 0.0) ? (1.0 / dT) : 0.0;
        r.cp_effective_J = effective_cp(r.f_liquid, df_dT,
                                        p.cp_solid_J, p.cp_liquid_J, p.dH_fusion_J);
    }
    else {
        r.f_liquid = sigmoid_liquid_fraction(T_K, p.T_melt_K, p.sigmoid_k);
        double df_dT = sigmoid_derivative(r.f_liquid, p.sigmoid_k);
        r.cp_effective_J = effective_cp(r.f_liquid, df_dT,
                                        p.cp_solid_J, p.cp_liquid_J, p.dH_fusion_J);
    }

    r.melt_score  = r.f_liquid;
    r.is_melting  = (r.f_liquid > 0.01 && r.f_liquid < 0.99);
    r.fully_liquid = (r.f_liquid >= 0.99);

    // Effective density
    if (p.rho_solid > 0.0 && p.rho_liquid > 0.0) {
        r.rho_effective = (1.0 - r.f_liquid) * p.rho_solid
                        + r.f_liquid * p.rho_liquid;
    }

    return r;
}

/**
 * Cubic EOS gas solver: Van der Waals.
 *   (P + a·n²/V²)(V - n·b) = nRT
 *
 * Newton iteration for V given T, P, n, a, b.
 * Mirrors gas2::vdw_solve_volume but returns phase::GasResult.
 */
inline GasResult vdw_gas(double T_K, double P_Pa, double n_mol,
                         double a, double b, int max_iter = 60) {
    GasResult r;
    r.mode  = PhaseModelMode::B_SMOOTHED_PHASE;
    r.T_K   = T_K;
    r.P_Pa  = P_Pa;
    r.n_mol = n_mol;

    double nRT = n_mol * R_gas_SI * T_K;
    double V   = nRT / P_Pa;  // ideal initial guess

    for (int it = 0; it < max_iter; ++it) {
        double nb  = n_mol * b;
        double n2a = n_mol * n_mol * a;
        double V2  = V * V;
        double V3  = V2 * V;

        double f  = P_Pa * V3 - (nb * P_Pa + nRT) * V2 + n2a * V - n2a * nb;
        double fp = 3.0 * P_Pa * V2 - 2.0 * (nb * P_Pa + nRT) * V + n2a;

        if (std::abs(fp) < 1e-30) break;
        double dV = f / fp;
        V -= dV;
        if (V < nb * 1.001) V = nb * 1.001;

        r.iterations = it + 1;
        if (std::abs(dV) < 1e-15 * std::abs(V)) {
            r.converged = true;
            break;
        }
    }

    r.V_m3 = V;
    r.Z    = (n_mol * R_gas_SI * T_K > 0.0)
             ? (P_Pa * V) / (n_mol * R_gas_SI * T_K)
             : 1.0;
    r.gas_nonideality = std::abs(r.Z - 1.0);
    r.fugacity_Pa     = P_Pa;  // VdW: approximate fugacity = P
    return r;
}

/**
 * Cubic EOS gas solver: Redlich-Kwong.
 *   P = RT/(Vm - b) - a / (sqrt(T) · Vm · (Vm + b))
 *
 * Vm = V/n (molar volume).  Newton iteration.
 */
inline GasResult rk_gas(double T_K, double P_Pa, double n_mol,
                        double a_RK, double b_RK, int max_iter = 60) {
    GasResult r;
    r.mode  = PhaseModelMode::B_SMOOTHED_PHASE;
    r.T_K   = T_K;
    r.P_Pa  = P_Pa;
    r.n_mol = n_mol;

    double sqrtT = std::sqrt(T_K);
    double Vm    = R_gas_SI * T_K / P_Pa;  // ideal molar volume guess

    for (int it = 0; it < max_iter; ++it) {
        double denom1 = Vm - b_RK;
        double denom2 = sqrtT * Vm * (Vm + b_RK);

        if (std::abs(denom1) < 1e-30 || std::abs(denom2) < 1e-30) break;

        double P_calc = R_gas_SI * T_K / denom1 - a_RK / denom2;
        double f = P_calc - P_Pa;

        // Derivative dP/dVm
        double dP = -R_gas_SI * T_K / (denom1 * denom1)
                    + a_RK * (2.0 * Vm + b_RK) / (sqrtT * Vm * Vm * (Vm + b_RK) * (Vm + b_RK));

        if (std::abs(dP) < 1e-30) break;
        double dVm = f / dP;
        Vm -= dVm;
        if (Vm < b_RK * 1.001) Vm = b_RK * 1.001;

        r.iterations = it + 1;
        if (std::abs(dVm) < 1e-15 * std::abs(Vm)) {
            r.converged = true;
            break;
        }
    }

    r.V_m3 = Vm * n_mol;
    r.Z    = (R_gas_SI * T_K > 0.0) ? (P_Pa * Vm) / (R_gas_SI * T_K) : 1.0;
    r.gas_nonideality = std::abs(r.Z - 1.0);
    r.fugacity_Pa     = P_Pa;  // RK: approximate
    return r;
}

} // namespace mode_b

// ============================================================================
// Mode C: Free-Energy / Stability-Competition Model (serious)
// ============================================================================

namespace mode_c {

/**
 * Gibbs free energy for a phase:
 *   G(T, P) = H_ref - T·S_ref + P·V
 *
 * With temperature-dependent corrections:
 *   G(T, P) = H_ref + cp·(T - T_ref) - T·(S_ref + cp·ln(T/T_ref)) + P·V
 *
 * Simplified (constant cp, P=0 reference):
 *   G(T) ≈ H_ref - T·S_ref
 */
inline double gibbs_simple(double H_ref, double S_ref, double T_K, double P_Pa, double V_m3) {
    return H_ref - T_K * S_ref + P_Pa * V_m3;
}

/**
 * Gibbs with temperature-dependent cp correction:
 *   G(T) = H_ref + cp·(T - Tref) - T·[S_ref + cp·ln(T/Tref)] + P·V
 */
inline double gibbs_with_cp(double H_ref, double S_ref, double cp,
                            double T_K, double T_ref, double P_Pa, double V_m3) {
    double dT = T_K - T_ref;
    double H = H_ref + cp * dT;
    double S = S_ref + cp * std::log(T_K / T_ref);
    return H - T_K * S + P_Pa * V_m3;
}

/**
 * Stability competition melt model.
 *
 * ΔG_melt = G_liquid - G_solid
 *   ΔG > 0 → solid favored
 *   ΔG < 0 → liquid favored
 *
 * Probabilistic liquid fraction:
 *   f_l = 1 / (1 + exp(ΔG / γ))
 */
inline MeltResult stability_melt(double T_K, double P_Pa, const PhaseParams& p) {
    MeltResult r;
    r.mode = PhaseModelMode::C_STABILITY_COMPETITION;
    r.T_K  = T_K;

    if (p.T_melt_K <= 0.0) {
        r.melt_score = 0.0;
        r.cp_effective_J = p.cp_solid_J;
        return r;
    }

    // Compute Gibbs free energy for each phase
    double T_ref = p.T_melt_K;  // Reference at melting point

    if (T_ref > 0.0 && T_K > 0.0) {
        r.G_solid_J  = gibbs_with_cp(p.H_solid_ref, p.S_solid_ref,
                                      p.cp_solid_J, T_K, T_ref, P_Pa, p.V_solid_m3);
        r.G_liquid_J = gibbs_with_cp(p.H_liquid_ref, p.S_liquid_ref,
                                      p.cp_liquid_J, T_K, T_ref, P_Pa, p.V_liquid_m3);
    }
    else {
        r.G_solid_J  = gibbs_simple(p.H_solid_ref, p.S_solid_ref, T_K, P_Pa, p.V_solid_m3);
        r.G_liquid_J = gibbs_simple(p.H_liquid_ref, p.S_liquid_ref, T_K, P_Pa, p.V_liquid_m3);
    }

    // Melt-driving potential
    r.dG_melt_J = r.G_liquid_J - r.G_solid_J;

    // Probabilistic liquid fraction via Boltzmann softening
    double gamma = std::max(p.gamma_J, 1.0);  // Prevent division by tiny number
    double x = r.dG_melt_J / gamma;
    if (x > 500.0)
        r.f_liquid = 0.0;
    else if (x < -500.0)
        r.f_liquid = 1.0;
    else
        r.f_liquid = 1.0 / (1.0 + std::exp(x));

    r.melt_score   = r.f_liquid;
    r.is_melting   = (r.f_liquid > 0.01 && r.f_liquid < 0.99);
    r.fully_liquid = (r.f_liquid >= 0.99);

    // Effective properties
    r.cp_effective_J = (1.0 - r.f_liquid) * p.cp_solid_J
                     + r.f_liquid * p.cp_liquid_J;

    if (p.rho_solid > 0.0 && p.rho_liquid > 0.0) {
        r.rho_effective = (1.0 - r.f_liquid) * p.rho_solid
                        + r.f_liquid * p.rho_liquid;
    }

    return r;
}

/**
 * Fugacity-based gas treatment.
 *   μ = μ°(T) + RT·ln(f)
 *   f = φ·P
 *
 * For Redlich-Kwong EOS, the fugacity coefficient is:
 *   ln(φ) = (Z-1) - ln(Z-B') - (A'²/B')·ln(1 + B'/Z)
 *
 * Where A' = aP/(R²T^2.5), B' = bP/(RT)
 *
 * Simplified: use Z from cubic EOS to estimate φ.
 */
inline GasResult fugacity_gas(double T_K, double P_Pa, double n_mol,
                              double a_RK, double b_RK, int max_iter = 60) {
    // First solve volume using RK
    GasResult r;
    r.mode  = PhaseModelMode::C_STABILITY_COMPETITION;
    r.T_K   = T_K;
    r.P_Pa  = P_Pa;
    r.n_mol = n_mol;

    double sqrtT = std::sqrt(T_K);
    double Vm    = R_gas_SI * T_K / P_Pa;

    for (int it = 0; it < max_iter; ++it) {
        double denom1 = Vm - b_RK;
        double denom2 = sqrtT * Vm * (Vm + b_RK);

        if (std::abs(denom1) < 1e-30 || std::abs(denom2) < 1e-30) break;

        double P_calc = R_gas_SI * T_K / denom1 - a_RK / denom2;
        double f = P_calc - P_Pa;

        double dP = -R_gas_SI * T_K / (denom1 * denom1)
                    + a_RK * (2.0 * Vm + b_RK) / (sqrtT * Vm * Vm * (Vm + b_RK) * (Vm + b_RK));

        if (std::abs(dP) < 1e-30) break;
        double dVm = f / dP;
        Vm -= dVm;
        if (Vm < b_RK * 1.001) Vm = b_RK * 1.001;

        r.iterations = it + 1;
        if (std::abs(dVm) < 1e-15 * std::abs(Vm)) {
            r.converged = true;
            break;
        }
    }

    r.V_m3 = Vm * n_mol;
    r.Z    = (R_gas_SI * T_K > 0.0) ? (P_Pa * Vm) / (R_gas_SI * T_K) : 1.0;
    r.gas_nonideality = std::abs(r.Z - 1.0);

    // Fugacity coefficient from RK EOS
    //   A' = a·P / (R²·T^2.5)
    //   B' = b·P / (R·T)
    //   ln(φ) = (Z-1) - ln(Z - B') - (A'/B')·ln(1 + B'/Z)
    double RT   = R_gas_SI * T_K;
    double RT25 = RT * RT * sqrtT / (P_Pa > 0 ? 1.0 : 1.0); // R²T^2.5
    double A_prime = (RT25 > 0.0) ? a_RK * P_Pa / (R_gas_SI * R_gas_SI * T_K * T_K * sqrtT) : 0.0;
    double B_prime = (RT > 0.0) ? b_RK * P_Pa / RT : 0.0;

    double Z_B = r.Z - B_prime;
    if (Z_B > 0.0 && B_prime > 0.0 && r.Z > 0.0) {
        double ln_phi = (r.Z - 1.0) - std::log(Z_B)
                      - (A_prime / B_prime) * std::log(1.0 + B_prime / r.Z);
        r.phi = std::exp(std::clamp(ln_phi, -20.0, 20.0));
    }
    else {
        r.phi = 1.0;  // Fallback to ideal
    }

    r.fugacity_Pa = r.phi * P_Pa;
    return r;
}

} // namespace mode_c

// ============================================================================
// Unified phase evaluation
// ============================================================================

/**
 * Compute phase risk score from melt result and temperature context.
 *   - Near melting: high risk
 *   - Near boiling: very high risk
 *   - In melt range: moderate risk
 */
inline double compute_phase_risk(const MeltResult& m, const PhaseParams& p) {
    double risk = 0.0;

    // Risk from proximity to melting
    if (p.T_melt_K > 0.0) {
        double frac = m.T_K / p.T_melt_K;
        if (frac > 0.85 && frac < 1.15)
            risk = std::max(risk, 0.7 + 0.3 * (1.0 - std::abs(frac - 1.0) / 0.15));
    }

    // Additional risk from being in the transition zone
    if (m.is_melting)
        risk = std::max(risk, 0.6);

    // Risk from proximity to boiling
    if (p.T_boil_K > 0.0) {
        double frac_boil = m.T_K / p.T_boil_K;
        if (frac_boil > 0.85)
            risk = std::max(risk, 0.85);
    }

    return std::clamp(risk, 0.0, 1.0);
}

/**
 * Compute temperature severity as normalized thermal stress.
 * Uses the highest relevant reference temperature.
 */
inline double compute_temperature_severity(double T_K, const PhaseParams& p) {
    double ref = 300.0;  // Room temperature fallback
    if (p.T_melt_K > 0.0) ref = p.T_melt_K;
    if (p.T_boil_K > 0.0) ref = std::max(ref, p.T_boil_K);

    return std::clamp(T_K / ref, 0.0, 1.0);
}

/**
 * Run full phase evaluation at a given temperature and pressure.
 *
 * Dispatches to the appropriate mode (A, B, or C) and produces
 * a unified PhaseReport with all four output variables.
 */
inline PhaseReport evaluate_phase(PhaseModelMode mode, double T_K, double P_Pa,
                                  double n_mol, const PhaseParams& p,
                                  bool use_piecewise_melt = false) {
    PhaseReport report;
    report.mode = mode;
    report.T_K  = T_K;

    switch (mode) {
        case PhaseModelMode::A_THRESHOLD_ENTHALPY: {
            report.melt = mode_a::threshold_melt(T_K, p);
            report.gas  = mode_a::ideal_gas_z(T_K, P_Pa, n_mol);
            break;
        }
        case PhaseModelMode::B_SMOOTHED_PHASE: {
            report.melt = mode_b::smoothed_melt(T_K, p, use_piecewise_melt);

            // Use VdW if parameters available, otherwise RK from critical constants
            if (p.vdw_a > 0.0 && p.vdw_b > 0.0) {
                report.gas = mode_b::vdw_gas(T_K, P_Pa, n_mol, p.vdw_a, p.vdw_b);
            }
            else if (p.Tc_K > 0.0 && p.Pc_Pa > 0.0) {
                double a_RK = 0.42748 * R_gas_SI * R_gas_SI * std::pow(p.Tc_K, 2.5) / p.Pc_Pa;
                double b_RK = 0.08664 * R_gas_SI * p.Tc_K / p.Pc_Pa;
                report.gas = mode_b::rk_gas(T_K, P_Pa, n_mol, a_RK, b_RK);
            }
            else {
                report.gas = mode_a::ideal_gas_z(T_K, P_Pa, n_mol);
                report.gas.mode = PhaseModelMode::B_SMOOTHED_PHASE;
            }
            break;
        }
        case PhaseModelMode::C_STABILITY_COMPETITION: {
            report.melt = mode_c::stability_melt(T_K, P_Pa, p);

            if (p.Tc_K > 0.0 && p.Pc_Pa > 0.0) {
                double a_RK = 0.42748 * R_gas_SI * R_gas_SI * std::pow(p.Tc_K, 2.5) / p.Pc_Pa;
                double b_RK = 0.08664 * R_gas_SI * p.Tc_K / p.Pc_Pa;
                report.gas = mode_c::fugacity_gas(T_K, P_Pa, n_mol, a_RK, b_RK);
            }
            else {
                report.gas = mode_a::ideal_gas_z(T_K, P_Pa, n_mol);
                report.gas.mode = PhaseModelMode::C_STABILITY_COMPETITION;
            }
            break;
        }
    }

    // Unified output variables
    report.melt_score            = report.melt.melt_score;
    report.gas_nonideality_score = report.gas.gas_nonideality;
    report.phase_risk_score      = compute_phase_risk(report.melt, p);
    report.temperature_severity  = compute_temperature_severity(T_K, p);

    return report;
}

} // namespace phase
} // namespace atomistic
