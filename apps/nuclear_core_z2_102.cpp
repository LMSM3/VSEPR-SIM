/**
 * nuclear_core_z2_102.cpp
 * -----------------------
 * VSEPR-SIM 4.0-Legacy-Beta — Nuclear Core Runner: Central Atom Z=2..102
 * C++23 Edition (N4950)
 *
 * Sweeps every element from He (Z=2) to Nobelium (Z=102) as the central
 * atomic species. For each element:
 *   1. Performs a full gas2 monatomic analysis (EOS + kinetic + thermal).
 *   2. Encodes complete nuclear properties from gas2_nuclear.hpp.
 *   3. Writes per-element Markdown report.
 *   4. Accumulates into a master LaTeX document and CSV summary.
 *   5. Produces a SpreadsheetML XML for Excel.
 *
 * Outputs (default: reports/nuclear_core_z2_102/):
 *   master_report.tex      — compilable LaTeX (booktabs, siunitx, longtable)
 *   data.xml               — SpreadsheetML (Excel XML)
 *   summary.csv            — flat CSV log
 *   elements/Z-NNN-SYM.md — individual Markdown per element
 *
 * Build:
 *   cmake --build build --target nuclear-core-z2-102
 *
 * Usage:
 *   nuclear-core-z2-102 [OPTIONS]
 *
 * Options:
 *   --T N       Gas-phase analysis temperature K (default: 1000)
 *   --P N       Gas-phase analysis pressure atm  (default: 1.0)
 *   --out DIR   Output directory (default: reports/nuclear_core_z2_102)
 *   --quiet     Suppress per-element progress
 *   --help      Show this help
 */

#include "gas2/gas2_engine.hpp"
#include "gas2/gas2_nuclear.hpp"
#include "multiscale/scale_bridge.hpp"
#include "version/version_manifest.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <functional>

namespace fs = std::filesystem;
using namespace vsepr::gas2;

// ============================================================================
// ANSI
// ============================================================================

namespace ansi {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* DIM     = "\033[2m";
    const char* RED     = "\033[91m";
    const char* GREEN   = "\033[92m";
    const char* YELLOW  = "\033[93m";
    const char* CYAN    = "\033[96m";
    const char* MAGENTA = "\033[95m";
    const char* WHITE   = "\033[97m";
}

// ============================================================================
// Formatting helpers
// ============================================================================

static std::string fmt_d(double v, int prec = 4) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

static std::string fmt_sci(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(prec) << v;
    return ss.str();
}

static std::string timestamp_now() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static std::string escape_latex(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '&':  out += "\\&";  break;
            case '%':  out += "\\%";  break;
            case '$':  out += "\\$";  break;
            case '#':  out += "\\#";  break;
            case '_':  out += "\\_";  break;
            case '{':  out += "\\{";  break;
            case '}':  out += "\\}";  break;
            case '~':  out += "\\textasciitilde{}"; break;
            case '^':  out += "\\textasciicircum{}"; break;
            default:   out += c;
        }
    }
    return out;
}

static std::string escape_xml(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
        }
    }
    return out;
}

// ============================================================================
// Derived analytics struct — all computed from NuclearSpecies + Gas2Analysis
// ============================================================================

struct ElementAnalytics {
    // Thermodynamic potentials at analysis T (ideal monatomic gas, per mol)
    // A(T) = Cv·T - T·(Cv·ln(T) + R·ln(V/n) + S0_offset)  (Sackur-Tetrode approximation)
    // We use the Sackur-Tetrode absolute entropy and then:
    //   G(T,P) = H - T·S  ;  H = Cp·T (from 0 K reference, monatomic ideal)
    //   S (Sackur-Tetrode): S = R·[ln( (2πmkT/h²)^(3/2) · kT/P ) + 5/2 ]
    double S_ST_Jmol;       // Sackur-Tetrode entropy S (J/mol·K)
    double H_Jmol;          // Enthalpy H = Cp·T  (J/mol)
    double G_Jmol;          // Gibbs free energy G = H - T·S  (J/mol)
    double A_Jmol;          // Helmholtz A = U - T·S = Cv·T - T·S  (J/mol)
    double mu_Jmol;         // Chemical potential μ = G/1 = G for pure substance (J/mol)

    // Quantum gas indicator
    double Lambda_m;        // de Broglie thermal wavelength (m)
    double nLambda3;        // occupation parameter n·Λ³ (dimensionless); >1 → quantum onset

    // Phase state indicators at analysis T
    bool   above_melt;      // T > Tm
    bool   above_boil;      // T > Tb
    bool   is_gas_at_STP;   // melting point is 0 (noble/gas-phase element)

    // Solid-state derived
    double alpha_th_m2s;    // thermal diffusivity (m²/s)
    double lindemann;       // Lindemann ratio × 1e3
    double wigner_seitz_pm; // Wigner-Seitz radius estimate (pm)
    double reduced_mass_H;  // reduced mass with proton (amu)
    double frenkel_eV;      // Frenkel pair formation energy (eV)
    double alpha_recoil_MeV;// nuclear recoil energy from α decay (MeV)

    // Speed ratios (useful cross-checks)
    double vrms_over_vsound; // v_rms / c_sound — always sqrt(3/γ)=sqrt(9/5) for monat.
    double vmean_over_vrms;  // v_mean / v_rms — always sqrt(8/(3π))

    // Momentum transfer on elastic collision with H (nuclear recoil, eV)
    // E_r = 4·m·M_H / (m+M_H)² × KE_trans (maximum head-on)
    double max_recoil_H_eV;
};

// ============================================================================
// Element result record
// ============================================================================

struct ElementResult {
    int     Z;
    Gas2Analysis gas;               // full gas2 analysis at T, P
    const NuclearSpecies* nuclear;  // nuclear encoding (always valid Z=2..102)
    ElementAnalytics analytics;     // derived quantities
};

// Compute analytics from an element result
static ElementAnalytics compute_analytics(const ElementResult& er) {
    constexpr double R  = 8.314462618;
    constexpr double kB = 1.380649e-23;
    constexpr double h  = 6.62607015e-34;
    constexpr double NA = 6.02214076e23;

    const auto& ns = *er.nuclear;
    const auto& ga = er.gas;

    ElementAnalytics an{};

    double T  = ga.T_K;
    double P  = ga.P_Pa;
    double m  = ns.molar_mass_kg() / NA;

    double Lambda = (m > 0.0 && T > 0.0)
        ? h / std::sqrt(2.0 * 3.14159265358979 * m * kB * T)
        : 1.0e-10;
    an.Lambda_m = Lambda;

    double n_density = (ga.eos_ideal.V_m3 > 0.0)
        ? 1.0 / (ga.eos_ideal.V_m3 / NA)
        : P / (kB * T);
    an.nLambda3 = n_density * Lambda * Lambda * Lambda;

    double V_per_mol = (ga.eos_ideal.V_m3 > 0.0) ? ga.eos_ideal.V_m3 : R * T / P;
    double arg = V_per_mol / (NA * Lambda * Lambda * Lambda);
    an.S_ST_Jmol = (arg > 0.0) ? R * (std::log(arg) + 2.5) : 0.0;

    an.H_Jmol  = ga.Cp_calc * T;
    an.G_Jmol  = an.H_Jmol - T * an.S_ST_Jmol;
    an.A_Jmol  = ga.Cv_calc * T - T * an.S_ST_Jmol;
    an.mu_Jmol = an.G_Jmol;

    an.above_melt    = (ns.melting_point_K > 0.0 && T > ns.melting_point_K);
    an.above_boil    = (ns.boiling_point_K > 0.0 && T > ns.boiling_point_K);
    an.is_gas_at_STP = (ns.melting_point_K == 0.0);

    an.alpha_th_m2s    = ns.thermal_diffusivity_m2s();
    an.lindemann       = ns.lindemann_ratio();
    an.wigner_seitz_pm = ns.wigner_seitz_radius_pm();
    an.reduced_mass_H  = ns.reduced_mass_with_H_amu();
    an.frenkel_eV      = ns.frenkel_pair_energy_eV();
    an.alpha_recoil_MeV = ns.alpha_recoil_MeV();

    an.vrms_over_vsound = (ga.c_sound > 0.0) ? ga.v_rms / ga.c_sound : 0.0;
    an.vmean_over_vrms  = (ga.v_rms  > 0.0) ? ga.v_mean / ga.v_rms  : 0.0;

    double mH_amu = 1.00794;
    double M_amu  = ns.molar_mass_g;
    double gamma_recoil = 4.0 * mH_amu * M_amu / ((mH_amu + M_amu) * (mH_amu + M_amu));
    constexpr double eV_per_J = 1.0 / 1.602176634e-19;
    an.max_recoil_H_eV = gamma_recoil * ga.ke_translational * eV_per_J;

    return an;
}

// ============================================================================
// Category color for console
// ============================================================================

static const char* category_color(NuclearPhaseCategory cat) {
    switch (cat) {
        case NuclearPhaseCategory::NobleGas:        return ansi::CYAN;
        case NuclearPhaseCategory::Actinide:        return ansi::YELLOW;
        case NuclearPhaseCategory::Lanthanide:      return ansi::MAGENTA;
        case NuclearPhaseCategory::TransitionMetal: return ansi::WHITE;
        case NuclearPhaseCategory::AlkaliMetal:
        case NuclearPhaseCategory::AlkalineEarth:   return ansi::GREEN;
        default:                                    return ansi::RESET;
    }
}

// ============================================================================
// Markdown per-element report
// ============================================================================

static std::string element_markdown(const ElementResult& er) {
    std::ostringstream md;
    const auto& ns = *er.nuclear;
    const auto& ga = er.gas;
    const auto& an = er.analytics;

    md << "# Element Z=" << er.Z << " — " << ns.symbol << " (" << ns.name << ")\n\n";
    md << "**Isotope:** " << ns.isotope_label << "  \n";
    md << "**Molar Mass:** " << fmt_d(ns.molar_mass_g, 4) << " g/mol  \n";
    md << "**Category:** " << nuclear_phase_category_name(ns.category) << "  \n";
    md << "**Crystal Phase:** " << ns.crystal_phase << "  \n";
    md << "**Decay Mode:** " << decay_mode_name(ns.decay_mode);
    if (ns.half_life_s > 0.0 && ns.half_life_s < 1.0e30)
        md << " (t½ = " << fmt_sci(ns.half_life_s) << " s)";
    md << "  \n\n";

    // Identity + atomic structure block
    md << "## Atomic Identity\n\n";
    md << "| Property | Value | Unit |\n";
    md << "|----------|-------|------|\n";
    md << "| Atomic number Z | " << er.Z << " | — |\n";
    md << "| Mass number A (primary) | " << ns.A << " | — |\n";
    md << "| Molar mass | " << fmt_d(ns.molar_mass_g, 6) << " | g/mol |\n";
    md << "| Atomic radius | " << fmt_d(ns.atomic_radius_pm, 0) << " | pm |\n";
    md << "| Wigner-Seitz radius (est.) | " << fmt_d(an.wigner_seitz_pm, 1) << " | pm |\n";
    md << "| Kinetic diameter | " << fmt_d(ns.d_kinetic_pm, 0) << " | pm |\n";
    md << "| Pauling electronegativity | "
       << (ns.electronegativity > 0.0 ? fmt_d(ns.electronegativity, 2) : "—") << " | — |\n";
    md << "| Electron affinity | "
       << (ns.electron_affinity_eV > 0.0 ? fmt_d(ns.electron_affinity_eV, 3) : "—")
       << " | eV |\n";
    md << "| First ionisation energy | " << fmt_d(ns.ionisation_eV, 3) << " | eV |\n";
    md << "| Reduced mass with H | " << fmt_d(an.reduced_mass_H, 4) << " | amu |\n\n";

    md << "## Nuclear Properties\n\n";
    md << "| Property | Value | Unit |\n";
    md << "|----------|-------|------|\n";
    md << "| Binding energy/nucleon | " << fmt_d(ns.binding_energy_MeV, 4) << " | MeV |\n";
    md << "| Fissility Z²/A | " << fmt_d(ns.fissility, 3) << " | — |\n";
    md << "| Displacement energy Ed | " << fmt_d(ns.Ed_eV, 1) << " | eV |\n";
    md << "| Frenkel pair energy | " << fmt_d(an.frenkel_eV, 1) << " | eV |\n";
    md << "| Fissile | " << (ns.fissile ? "Yes" : "No") << " | — |\n";
    md << "| Fertile | " << (ns.fertile ? "Yes" : "No") << " | — |\n";
    md << "| Thermal neutron cross-section σ | "
       << (ns.sigma_thermal_b > 0.0 ? fmt_d(ns.sigma_thermal_b, 3) : "—")
       << " | barn |\n";
    md << "| Atomic mass excess Δ | " << fmt_d(ns.mass_excess_keV, 1) << " | keV |\n";
    md << "| Neutron separation energy Sn | "
       << (ns.Sn_keV > 0.0 ? fmt_d(ns.Sn_keV, 1) : "—") << " | keV |\n";
    if (ns.decay_mode == DecayMode::Alpha)
        md << "| Alpha recoil energy | " << fmt_d(an.alpha_recoil_MeV, 3) << " | MeV |\n";
    md << "\n";

    md << "## Thermal / Solid Properties\n\n";
    md << "| Property | Value | Unit |\n";
    md << "|----------|-------|------|\n";
    md << "| Melting point | " << fmt_d(ns.melting_point_K, 1) << " | K |\n";
    md << "| Boiling point | " << fmt_d(ns.boiling_point_K, 1) << " | K |\n";
    md << "| Thermal conductivity k | " << fmt_d(ns.k_thermal_W_mK, 3) << " | W/(m·K) |\n";
    md << "| Specific heat (solid) | " << fmt_d(ns.Cp_solid_J_kgK, 1) << " | J/(kg·K) |\n";
    md << "| Debye temperature | "
       << (ns.T_debye_K > 0.0 ? fmt_d(ns.T_debye_K, 0) : "—") << " | K |\n";
    md << "| Electrical resistivity | "
       << (ns.resistivity_nOhm_m > 0.0 && ns.resistivity_nOhm_m < 1e7
               ? fmt_d(ns.resistivity_nOhm_m, 1) : "—")
       << " | nΩ·m |\n";
    md << "| Thermal diffusivity | "
       << (an.alpha_th_m2s > 0.0 ? fmt_sci(an.alpha_th_m2s) : "—") << " | m²/s |\n";
    md << "| Lindemann ratio × 10³ | "
       << (an.lindemann > 0.0 ? fmt_d(an.lindemann, 3) : "—") << " | — |\n\n";

    md << "## Gas-Phase Analysis (T = " << fmt_d(ga.T_K, 0) << " K, P = "
       << fmt_d(ga.P_Pa / atm_to_Pa, 2) << " atm)\n\n";
    md << "**Phase state at analysis T:** ";
    if (an.is_gas_at_STP)         md << "Gas at STP  \n";
    else if (an.above_boil)       md << "Above boiling point → vapour  \n";
    else if (an.above_melt)       md << "Above melting point → liquid  \n";
    else                          md << "Below melting point → solid  \n";
    md << "\n";

    md << "### Equation of State\n\n";
    md << "| Property | Ideal | VdW | R-K | Unit |\n";
    md << "|----------|-------|-----|-----|------|\n";
    md << "| Molar volume | " << fmt_d(ga.eos_ideal.V_L(), 4)
       << " | " << fmt_d(ga.eos_vdw.V_L(), 4)
       << " | " << fmt_d(ga.eos_rk.V_L(), 4) << " | L/mol |\n";
    md << "| Z (compressibility) | " << fmt_d(ga.eos_ideal.Z, 5)
       << " | " << fmt_d(ga.eos_vdw.Z, 5)
       << " | " << fmt_d(ga.eos_rk.Z, 5) << " | — |\n";
    md << "| VdW deviation ΔV | — | " << fmt_d(ga.eos_vdw.V_L() - ga.eos_ideal.V_L(), 5)
       << " | " << fmt_d(ga.eos_rk.V_L() - ga.eos_ideal.V_L(), 5) << " | L/mol |\n\n";

    md << "### Kinetic Theory (Maxwell-Boltzmann)\n\n";
    md << "| Property | Value | Unit |\n";
    md << "|----------|-------|------|\n";
    md << "| Cv (monatomic) | " << fmt_d(ga.Cv_calc, 4) << " | J/(mol·K) |\n";
    md << "| Cp (monatomic) | " << fmt_d(ga.Cp_calc, 4) << " | J/(mol·K) |\n";
    md << "| γ = Cp/Cv | " << fmt_d(ga.gamma_calc, 5) << " | — |\n";
    md << "| v_rms | " << fmt_d(ga.v_rms, 2) << " | m/s |\n";
    md << "| v_mean | " << fmt_d(ga.v_mean, 2) << " | m/s |\n";
    md << "| v_mp (most probable) | " << fmt_d(ga.v_mp, 2) << " | m/s |\n";
    md << "| v_rms / c_sound | " << fmt_d(an.vrms_over_vsound, 4) << " | — |\n";
    md << "| v_mean / v_rms | " << fmt_d(an.vmean_over_vrms, 5) << " | — |\n";
    md << "| Sound speed c_s | " << fmt_d(ga.c_sound, 2) << " | m/s |\n";
    md << "| Mean free path λ | " << fmt_sci(ga.mean_free_path_m) << " | m |\n";
    md << "| KE translational | " << fmt_sci(ga.ke_translational) << " | J/molecule |\n";
    md << "| KE translational | " << fmt_sci(ga.ke_translational_Eh) << " | Eh |\n";
    md << "| Max recoil on H (elastic) | " << fmt_sci(an.max_recoil_H_eV) << " | eV |\n\n";

    md << "### Thermodynamic Potentials (Sackur-Tetrode / Ideal Monatomic)\n\n";
    md << "| Quantity | Value | Unit |\n";
    md << "|---------|-------|------|\n";
    md << "| Entropy S (Sackur-Tetrode) | " << fmt_d(an.S_ST_Jmol, 3) << " | J/(mol·K) |\n";
    md << "| Enthalpy H = Cp·T | " << fmt_sci(an.H_Jmol) << " | J/mol |\n";
    md << "| Gibbs energy G = H − T·S | " << fmt_sci(an.G_Jmol) << " | J/mol |\n";
    md << "| Helmholtz A = Cv·T − T·S | " << fmt_sci(an.A_Jmol) << " | J/mol |\n";
    md << "| Chemical potential μ = G | " << fmt_sci(an.mu_Jmol) << " | J/mol |\n\n";

    md << "### Quantum Gas Indicator\n\n";
    md << "| Quantity | Value | Interpretation |\n";
    md << "|---------|-------|----------------|\n";
    md << "| de Broglie Λ | " << fmt_sci(an.Lambda_m) << " m |";
    md << " thermal wavelength at T=" << fmt_d(ga.T_K, 0) << " K |\n";
    md << "| n·Λ³ | " << fmt_sci(an.nLambda3) << " |";
    md << (an.nLambda3 > 1.0 ? " **QUANTUM ONSET** (classical breakdown)" : " classical regime") << " |\n\n";

    // Scale bridge note for actinides
    if (ns.category == NuclearPhaseCategory::Actinide) {
        md << "## Scale Bridge Note (Actinide Domain)\n\n";
        md << "Displacement energy Ed = " << fmt_d(ns.Ed_eV, 1) << " eV ";
        md << "propagates Scale 1 (atomistic) → Scale 2 (CG) → Scale 3 (grain).\n";
        md << "Frenkel pair production rate scales with neutron flux × σ_displacement.\n";
        if (ns.sigma_thermal_b > 0.0)
            md << "Thermal neutron cross-section σ = " << fmt_d(ns.sigma_thermal_b, 2)
               << " barn (ENDF proxy).\n";
        if (ns.fissile) md << "**Fissile species:** thermal neutron cross-section active.\n";
        if (ns.fertile) md << "**Fertile species:** neutron capture breeding pathway active.\n";
        md << "\n";
    }

    md << "---\n";
    md << "*Generated by VSEPR-SIM nuclear-core-z2-102 (C++23 / N4950). "
       << "Data: IUPAC 2021, NUBASE2020, AME2020, NIST WebBook, ENDF/B-VIII, "
       << "CRC Handbook, ASTM E521.*\n";
    return md.str();
}

// ============================================================================
// LaTeX writer
// ============================================================================

class LaTeXWriter {
public:
    static std::string preamble(const std::string& timestamp, int count) {
        std::ostringstream tex;
        tex << "\\documentclass[11pt,a4paper]{article}\n";
        tex << "\\usepackage[utf8]{inputenc}\n";
        tex << "\\usepackage[T1]{fontenc}\n";
        tex << "\\usepackage{lmodern}\n";
        tex << "\\usepackage{geometry}\n";
        tex << "\\geometry{margin=2.5cm}\n";
        tex << "\\usepackage{booktabs}\n";
        tex << "\\usepackage{longtable}\n";
        tex << "\\usepackage{siunitx}\n";
        tex << "\\usepackage{hyperref}\n";
        tex << "\\usepackage{xcolor}\n";
        tex << "\\usepackage{fancyhdr}\n";
        tex << "\\pagestyle{fancy}\n";
        tex << "\\fancyhead[L]{VSEPR-SIM 4.0-LB (C++23 / N4950)}\n";
        tex << "\\fancyhead[R]{Nuclear Core Z=2..102}\n";
        tex << "\\fancyfoot[C]{\\thepage}\n";
        tex << "\\sisetup{output-exponent-marker=\\ensuremath{\\mathrm{e}}}\n\n";
        tex << "\\title{Nuclear Core Runner: Central Atom Z=2 (He) through Z=102 (No)\\\\\n";
        tex << "       \\large Complete Elemental Encoding --- VSEPR-SIM 4.0-LB}\n";
        tex << "\\author{VSEPR-SIM Autonomous Nuclear Core Engine}\n";
        tex << "\\date{" << escape_latex(timestamp) << "}\n\n";
        tex << "\\begin{document}\n";
        tex << "\\maketitle\n\n";
        tex << "\\begin{abstract}\n";
        tex << "Complete nuclear species encoding sweep from Z=2 (Helium) through Z=102 "
               "(Nobelium). "
               "Each element is analysed as a monatomic gas-phase species using the gas2 "
               "three-EOS pipeline (ideal, Van der Waals, Redlich--Kwong), kinetic theory "
               "(Maxwell--Boltzmann), and the full heat-capacity framework. "
               "Nuclear properties (binding energy, fissility, displacement energy, "
               "decay mode, half-life, neutron cross-section, mass excess, "
               "neutron separation energy) are encoded from the gas2\\_nuclear database "
               "(NUBASE2020, AME2020, IUPAC 2021, ASTM E521, ENDF/B-VIII, CRC Handbook). "
               "Solid-state fields include Debye temperature, atomic radius, "
               "Pauling electronegativity, electron affinity, and electrical resistivity. "
               "Derived analytics include Sackur--Tetrode entropy, Gibbs and Helmholtz "
               "free energies, de Broglie thermal wavelength, Lindemann ratio, "
               "thermal diffusivity, Frenkel pair energies, and nuclear recoil estimates. "
               "Total elements: " << count << ".\n";
        tex << "\\end{abstract}\n";
        tex << "\\tableofcontents\n";
        tex << "\\newpage\n\n";
        return tex.str();
    }

    static std::string summary_table(const std::vector<ElementResult>& results) {
        std::ostringstream tex;
        tex << "\\section{Summary Table: All Elements Z=2..102}\n\n";
        tex << "\\begin{longtable}{rllllrrrrrr}\n";
        tex << "  \\toprule\n";
        tex << "  Z & Sym & Name & Cat. & Decay & $M$ & "
               "$T_m$ (K) & $B/A$ (MeV) & $\\chi$ & $\\sigma_{\\rm th}$ (b) & "
               "$v_{\\rm rms}$ (m/s)\\\\\n";
        tex << "  \\midrule\n";
        tex << "  \\endhead\n";
        for (const auto& er : results) {
            const auto& ns = *er.nuclear;
            tex << "  " << er.Z
                << " & " << escape_latex(ns.symbol)
                << " & " << escape_latex(ns.name)
                << " & " << escape_latex(std::string(nuclear_phase_category_name(ns.category))).substr(0, 4)
                << " & " << escape_latex(std::string(decay_mode_name(ns.decay_mode)))
                << " & " << fmt_d(ns.molar_mass_g, 3)
                << " & " << fmt_d(ns.melting_point_K, 0)
                << " & " << fmt_d(ns.binding_energy_MeV, 4)
                << " & " << (ns.electronegativity > 0 ? fmt_d(ns.electronegativity, 2) : "---")
                << " & " << (ns.sigma_thermal_b > 0 ? fmt_d(ns.sigma_thermal_b, 2) : "---")
                << " & " << fmt_d(er.gas.v_rms, 0)
                << " \\\\\n";
        }
        tex << "  \\bottomrule\n";
        tex << "\\end{longtable}\n\n";

        // Thermodynamic potential summary table
        tex << "\\section{Thermodynamic Potential Summary at Analysis T}\n\n";
        tex << "\\begin{longtable}{rllrrrr}\n";
        tex << "  \\toprule\n";
        tex << "  Z & Sym & Phase & $S_{\\rm ST}$ (J/mol/K) & "
               "$G$ (J/mol) & $A$ (J/mol) & $n\\Lambda^3$\\\\\n";
        tex << "  \\midrule\n";
        tex << "  \\endhead\n";
        for (const auto& er : results) {
            const auto& ns = *er.nuclear;
            const auto& an = er.analytics;
            std::string phase = an.is_gas_at_STP ? "gas" : an.above_boil ? "vap" : an.above_melt ? "liq" : "sol";
            tex << "  " << er.Z
                << " & " << escape_latex(ns.symbol)
                << " & " << phase
                << " & " << fmt_d(an.S_ST_Jmol, 2)
                << " & " << fmt_sci(an.G_Jmol)
                << " & " << fmt_sci(an.A_Jmol)
                << " & " << fmt_sci(an.nLambda3)
                << " \\\\\n";
        }
        tex << "  \\bottomrule\n";
        tex << "\\end{longtable}\n\n";
        tex << "\\newpage\n\n";
        return tex.str();
    }

    static std::string element_section(const ElementResult& er) {
        std::ostringstream tex;
        const auto& ns = *er.nuclear;
        const auto& ga = er.gas;

        tex << "\\section{Z=" << er.Z << " --- " << escape_latex(ns.symbol)
            << " (" << escape_latex(ns.name) << ")}\n";
        tex << "\\label{sec:Z" << er.Z << "}\n\n";

        // Identity
        tex << "\\subsection{Identity}\n";
        tex << "\\begin{tabular}{ll}\n";
        tex << "  \\toprule\n";
        tex << "  Field & Value \\\\\n";
        tex << "  \\midrule\n";
        tex << "  Symbol & " << escape_latex(ns.symbol) << " \\\\\n";
        tex << "  Name & " << escape_latex(ns.name) << " \\\\\n";
        tex << "  Isotope & " << escape_latex(ns.isotope_label) << " \\\\\n";
        tex << "  Molar mass & \\SI{" << fmt_d(ns.molar_mass_g, 4) << "}{g/mol} \\\\\n";
        tex << "  Category & " << escape_latex(std::string(nuclear_phase_category_name(ns.category))) << " \\\\\n";
        tex << "  Crystal phase & " << escape_latex(ns.crystal_phase) << " \\\\\n";
        tex << "  Decay mode & " << escape_latex(std::string(decay_mode_name(ns.decay_mode)));
        if (ns.half_life_s > 0.0 && ns.half_life_s < 1.0e30)
            tex << " ($t_{1/2}$=" << fmt_sci(ns.half_life_s) << " s)";
        tex << " \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";

        // Nuclear properties
        tex << "\\subsection{Nuclear Properties}\n";
        tex << "\\begin{tabular}{lrl}\n";
        tex << "  \\toprule\n";
        tex << "  Property & Value & Unit \\\\\n";
        tex << "  \\midrule\n";
        tex << "  Binding energy per nucleon & " << fmt_d(ns.binding_energy_MeV, 4) << " & \\si{MeV} \\\\\n";
        tex << "  Fissility $Z^2/A$ & " << fmt_d(ns.fissility, 3) << " & --- \\\\\n";
        tex << "  Displacement energy $E_d$ & " << fmt_d(ns.Ed_eV, 1) << " & \\si{eV} \\\\\n";
        tex << "  First ionisation & " << fmt_d(ns.ionisation_eV, 3) << " & \\si{eV} \\\\\n";
        tex << "  Fissile & " << (ns.fissile ? "Yes" : "No") << " & --- \\\\\n";
        tex << "  Fertile & " << (ns.fertile ? "Yes" : "No") << " & --- \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";

        // Solid thermal
        tex << "\\subsection{Solid-Phase Thermal}\n";
        tex << "\\begin{tabular}{lrl}\n";
        tex << "  \\toprule\n";
        tex << "  Property & Value & Unit \\\\\n";
        tex << "  \\midrule\n";
        tex << "  Melting point & " << fmt_d(ns.melting_point_K, 0) << " & \\si{K} \\\\\n";
        tex << "  Boiling point & " << fmt_d(ns.boiling_point_K, 0) << " & \\si{K} \\\\\n";
        tex << "  Thermal conductivity & " << fmt_d(ns.k_thermal_W_mK, 3) << " & \\si{W/(m.K)} \\\\\n";
        tex << "  Specific heat (solid) & " << fmt_d(ns.Cp_solid_J_kgK, 1) << " & \\si{J/(kg.K)} \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";

        // Gas-phase EOS
        tex << "\\subsection{Gas-Phase Analysis at T=\\SI{" << fmt_d(ga.T_K, 0)
            << "}{K}, P=\\SI{" << fmt_d(ga.P_Pa / atm_to_Pa, 2) << "}{atm}}\n";
        tex << "\\begin{tabular}{lrrrr}\n";
        tex << "  \\toprule\n";
        tex << "  Property & Ideal & VdW & R-K & Unit \\\\\n";
        tex << "  \\midrule\n";
        tex << "  Molar volume & " << fmt_d(ga.eos_ideal.V_L(), 4)
            << " & " << fmt_d(ga.eos_vdw.V_L(), 4)
            << " & " << fmt_d(ga.eos_rk.V_L(), 4) << " & \\si{L/mol} \\\\\n";
        tex << "  Compressibility $Z$ & " << fmt_d(ga.eos_ideal.Z, 4)
            << " & " << fmt_d(ga.eos_vdw.Z, 4)
            << " & " << fmt_d(ga.eos_rk.Z, 4) << " & --- \\\\\n";
        tex << "  \\midrule\n";
        tex << "  $C_v$ (monatomic) & \\multicolumn{3}{c}{" << fmt_d(ga.Cv_calc, 3)
            << "} & \\si{J/(mol.K)} \\\\\n";
        tex << "  $C_p$ (monatomic) & \\multicolumn{3}{c}{" << fmt_d(ga.Cp_calc, 3)
            << "} & \\si{J/(mol.K)} \\\\\n";
        tex << "  $\\gamma = C_p/C_v$ & \\multicolumn{3}{c}{" << fmt_d(ga.gamma_calc, 4)
            << "} & --- \\\\\n";
        tex << "  $v_{\\rm rms}$ & \\multicolumn{3}{c}{" << fmt_d(ga.v_rms, 1)
            << "} & \\si{m/s} \\\\\n";
        tex << "  $v_{\\rm mean}$ & \\multicolumn{3}{c}{" << fmt_d(ga.v_mean, 1)
            << "} & \\si{m/s} \\\\\n";
        tex << "  Sound speed & \\multicolumn{3}{c}{" << fmt_d(ga.c_sound, 1)
            << "} & \\si{m/s} \\\\\n";
        tex << "  Mean free path & \\multicolumn{3}{c}{" << fmt_sci(ga.mean_free_path_m)
            << "} & \\si{m} \\\\\n";
        tex << "  $\\langle KE \\rangle_{\\rm trans}$ & \\multicolumn{3}{c}{"
            << fmt_sci(ga.ke_translational) << "} & \\si{J/molecule} \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";

        // Thermodynamic potentials
        const auto& an = er.analytics;
        tex << "\\subsection{Thermodynamic Potentials (Sackur--Tetrode, Ideal Monatomic)}\n";
        tex << "\\begin{tabular}{lrl}\n";
        tex << "  \\toprule\n";
        tex << "  Quantity & Value & Unit \\\\\n";
        tex << "  \\midrule\n";
        tex << "  Entropy $S_{\\rm ST}$ & " << fmt_d(an.S_ST_Jmol, 3) << " & \\si{J/(mol.K)} \\\\\n";
        tex << "  Enthalpy $H = C_p T$ & " << fmt_sci(an.H_Jmol) << " & \\si{J/mol} \\\\\n";
        tex << "  Gibbs $G = H - TS$ & " << fmt_sci(an.G_Jmol) << " & \\si{J/mol} \\\\\n";
        tex << "  Helmholtz $A = U - TS$ & " << fmt_sci(an.A_Jmol) << " & \\si{J/mol} \\\\\n";
        tex << "  Chemical potential $\\mu$ & " << fmt_sci(an.mu_Jmol) << " & \\si{J/mol} \\\\\n";
        tex << "  \\midrule\n";
        tex << "  de Broglie $\\Lambda$ & " << fmt_sci(an.Lambda_m) << " & \\si{m} \\\\\n";
        tex << "  $n\\Lambda^3$ (quantum indicator) & " << fmt_sci(an.nLambda3) << " & --- \\\\\n";
        tex << "  Phase at $T_{\\rm anal}$ & "
            << (an.is_gas_at_STP ? "Gas (STP)" : an.above_boil ? "Vapour" : an.above_melt ? "Liquid" : "Solid")
            << " & --- \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";

        // Extended atomic / solid-state
        tex << "\\subsection{Extended Atomic and Solid-State Properties}\n";
        tex << "\\begin{tabular}{lrl}\n";
        tex << "  \\toprule\n";
        tex << "  Property & Value & Unit \\\\\n";
        tex << "  \\midrule\n";
        tex << "  Atomic radius & " << fmt_d(ns.atomic_radius_pm, 0) << " & \\si{pm} \\\\\n";
        tex << "  Wigner-Seitz $r_s$ (est.) & " << fmt_d(an.wigner_seitz_pm, 1) << " & \\si{pm} \\\\\n";
        tex << "  Pauling electronegativity & "
            << (ns.electronegativity > 0 ? fmt_d(ns.electronegativity, 2) : "---") << " & --- \\\\\n";
        tex << "  Electron affinity & "
            << (ns.electron_affinity_eV > 0 ? fmt_d(ns.electron_affinity_eV, 3) : "---") << " & \\si{eV} \\\\\n";
        tex << "  Debye temperature & "
            << (ns.T_debye_K > 0 ? fmt_d(ns.T_debye_K, 0) : "---") << " & \\si{K} \\\\\n";
        tex << "  Electrical resistivity & "
            << (ns.resistivity_nOhm_m > 0 && ns.resistivity_nOhm_m < 1e7
                ? fmt_d(ns.resistivity_nOhm_m, 1) : "---") << " & \\si{n\\Omega\\cdot m} \\\\\n";
        tex << "  Thermal diffusivity $\\alpha$ & "
            << (an.alpha_th_m2s > 0 ? fmt_sci(an.alpha_th_m2s) : "---") << " & \\si{m^2/s} \\\\\n";
        tex << "  Lindemann ratio $\\times 10^3$ & "
            << (an.lindemann > 0 ? fmt_d(an.lindemann, 3) : "---") << " & --- \\\\\n";
        tex << "  $\\sigma_{\\rm th}$ (neutron) & "
            << (ns.sigma_thermal_b > 0 ? fmt_d(ns.sigma_thermal_b, 3) : "---") << " & \\si{b} \\\\\n";
        tex << "  Mass excess $\\Delta$ & " << fmt_d(ns.mass_excess_keV, 1) << " & \\si{keV} \\\\\n";
        tex << "  Neutron sep. energy $S_n$ & "
            << (ns.Sn_keV > 0 ? fmt_d(ns.Sn_keV, 1) : "---") << " & \\si{keV} \\\\\n";
        tex << "  Reduced mass with H & " << fmt_d(an.reduced_mass_H, 4) << " & \\si{amu} \\\\\n";
        tex << "  Frenkel pair energy & " << fmt_d(an.frenkel_eV, 1) << " & \\si{eV} \\\\\n";
        if (ns.decay_mode == DecayMode::Alpha)
            tex << "  $\\alpha$ recoil energy & " << fmt_d(an.alpha_recoil_MeV, 3) << " & \\si{MeV} \\\\\n";
        tex << "  Max recoil on H & " << fmt_sci(an.max_recoil_H_eV) << " & \\si{eV} \\\\\n";
        tex << "  \\bottomrule\n";
        tex << "\\end{tabular}\n\n";

        tex << "\\newpage\n\n";
        return tex.str();
    }

    static std::string footer() {
        return "\\end{document}\n";
    }
};

// ============================================================================
// Excel XML writer
// ============================================================================

class ExcelXMLWriter {
public:
    ExcelXMLWriter() = default;

    void add_header() {
        rows_.push_back({
            "Z", "Symbol", "Name", "Isotope", "Category", "Decay", "Molar_Mass_g",
            "Melting_K", "Boiling_K", "k_W_mK", "Cp_solid_J_kgK",
            "T_debye_K", "Atomic_radius_pm", "Electronegativity", "Electron_affinity_eV",
            "Resistivity_nOhm_m",
            "Binding_MeV", "Fissility", "Ed_eV", "Ionisation_eV",
            "Sigma_th_barn", "Mass_excess_keV", "Sn_keV",
            "Fissile", "Fertile", "Half_life_s",
            "T_analysis_K", "P_atm",
            "V_ideal_L", "V_vdw_L", "V_rk_L",
            "Z_ideal", "Z_vdw", "Z_rk",
            "Cv_Jmol", "Cp_Jmol", "gamma",
            "v_rms_ms", "v_mean_ms", "v_mp_ms",
            "c_sound_ms", "MFP_m", "KE_trans_J",
            "S_ST_Jmol", "H_Jmol", "G_Jmol", "A_Jmol",
            "Lambda_m", "nLambda3",
            "Thermal_diffusivity_m2s", "Lindemann_x1e3",
            "Frenkel_pair_eV", "Reduced_mass_H_amu",
            "Max_recoil_H_eV", "Phase_at_T",
            "Alpha_recoil_MeV", "Wigner_Seitz_pm"
        });
    }

    void add_element(const ElementResult& er) {
        const auto& ns = *er.nuclear;
        const auto& ga = er.gas;
        const auto& an = er.analytics;
        std::string phase = an.is_gas_at_STP ? "gas" : an.above_boil ? "vapour" : an.above_melt ? "liquid" : "solid";
        rows_.push_back({
            std::to_string(er.Z),
            std::string(ns.symbol),
            std::string(ns.name),
            std::string(ns.isotope_label),
            std::string(nuclear_phase_category_name(ns.category)),
            std::string(decay_mode_name(ns.decay_mode)),
            fmt_d(ns.molar_mass_g, 4),
            fmt_d(ns.melting_point_K, 1),
            fmt_d(ns.boiling_point_K, 1),
            fmt_d(ns.k_thermal_W_mK, 4),
            fmt_d(ns.Cp_solid_J_kgK, 2),
            fmt_d(ns.T_debye_K, 0),
            fmt_d(ns.atomic_radius_pm, 0),
            fmt_d(ns.electronegativity, 3),
            fmt_d(ns.electron_affinity_eV, 4),
            fmt_d(ns.resistivity_nOhm_m, 2),
            fmt_d(ns.binding_energy_MeV, 5),
            fmt_d(ns.fissility, 4),
            fmt_d(ns.Ed_eV, 1),
            fmt_d(ns.ionisation_eV, 4),
            fmt_d(ns.sigma_thermal_b, 4),
            fmt_d(ns.mass_excess_keV, 2),
            fmt_d(ns.Sn_keV, 2),
            ns.fissile ? "1" : "0",
            ns.fertile ? "1" : "0",
            fmt_sci(ns.half_life_s),
            fmt_d(ga.T_K, 1),
            fmt_d(ga.P_Pa / atm_to_Pa, 3),
            fmt_d(ga.eos_ideal.V_L(), 5),
            fmt_d(ga.eos_vdw.V_L(), 5),
            fmt_d(ga.eos_rk.V_L(), 5),
            fmt_d(ga.eos_ideal.Z, 5),
            fmt_d(ga.eos_vdw.Z, 5),
            fmt_d(ga.eos_rk.Z, 5),
            fmt_d(ga.Cv_calc, 4),
            fmt_d(ga.Cp_calc, 4),
            fmt_d(ga.gamma_calc, 5),
            fmt_d(ga.v_rms, 2),
            fmt_d(ga.v_mean, 2),
            fmt_d(ga.v_mp, 2),
            fmt_d(ga.c_sound, 2),
            fmt_sci(ga.mean_free_path_m),
            fmt_sci(ga.ke_translational),
            fmt_d(an.S_ST_Jmol, 3),
            fmt_sci(an.H_Jmol),
            fmt_sci(an.G_Jmol),
            fmt_sci(an.A_Jmol),
            fmt_sci(an.Lambda_m),
            fmt_sci(an.nLambda3),
            an.alpha_th_m2s > 0 ? fmt_sci(an.alpha_th_m2s) : "0",
            an.lindemann > 0    ? fmt_d(an.lindemann, 4) : "0",
            fmt_d(an.frenkel_eV, 2),
            fmt_d(an.reduced_mass_H, 5),
            fmt_sci(an.max_recoil_H_eV),
            phase,
            fmt_d(an.alpha_recoil_MeV, 4),
            fmt_d(an.wigner_seitz_pm, 1)
        });
    }

    std::string to_xml() const {
        std::ostringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xml << "<?mso-application progid=\"Excel.Sheet\"?>\n";
        xml << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n";
        xml << "  xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n";
        xml << "  <Styles>\n";
        xml << "    <Style ss:ID=\"header\">\n";
        xml << "      <Font ss:Bold=\"1\" ss:Size=\"10\"/>\n";
        xml << "      <Interior ss:Color=\"#1F3864\" ss:Pattern=\"Solid\"/>\n";
        xml << "      <Font ss:Color=\"#FFFFFF\" ss:Bold=\"1\"/>\n";
        xml << "    </Style>\n";
        xml << "    <Style ss:ID=\"actinide\">\n";
        xml << "      <Interior ss:Color=\"#FFF2CC\" ss:Pattern=\"Solid\"/>\n";
        xml << "    </Style>\n";
        xml << "    <Style ss:ID=\"lanthanide\">\n";
        xml << "      <Interior ss:Color=\"#E2EFDA\" ss:Pattern=\"Solid\"/>\n";
        xml << "    </Style>\n";
        xml << "    <Style ss:ID=\"noble\">\n";
        xml << "      <Interior ss:Color=\"#DAEEF3\" ss:Pattern=\"Solid\"/>\n";
        xml << "    </Style>\n";
        xml << "    <Style ss:ID=\"default\"/>\n";
        xml << "  </Styles>\n";
        xml << "  <Worksheet ss:Name=\"Nuclear Core Z2-102\">\n";
        xml << "    <Table>\n";

        for (size_t r = 0; r < rows_.size(); ++r) {
            bool is_header = (r == 0);
            std::string style = "default";
            if (is_header) style = "header";
            else {
                // Determine style from category column (index 4)
                const auto& cat = rows_[r][4];
                if (cat.find("Actinide") != std::string::npos) style = "actinide";
                else if (cat.find("Lanthanide") != std::string::npos) style = "lanthanide";
                else if (cat.find("Noble") != std::string::npos) style = "noble";
            }

            xml << "      <Row>\n";
            for (const auto& cell : rows_[r]) {
                bool numeric = false;
                if (!is_header && !cell.empty()) {
                    char* end = nullptr;
                    std::strtod(cell.c_str(), &end);
                    numeric = (end != cell.c_str() && *end == '\0');
                }
                xml << "        <Cell ss:StyleID=\"" << style << "\">";
                if (numeric)
                    xml << "<Data ss:Type=\"Number\">" << cell << "</Data>";
                else
                    xml << "<Data ss:Type=\"String\">" << escape_xml(cell) << "</Data>";
                xml << "</Cell>\n";
            }
            xml << "      </Row>\n";
        }

        xml << "    </Table>\n";
        xml << "  </Worksheet>\n";
        xml << "</Workbook>\n";
        return xml.str();
    }

private:
    std::vector<std::vector<std::string>> rows_;
};

// ============================================================================
// Runner
// ============================================================================

struct Config {
    double      T_K        = 1000.0;
    double      P_atm      = 1.0;
    std::string output_dir = "reports/nuclear_core_z2_102";
    bool        quiet      = false;
    bool        json       = true;                          // emit data.json
    std::vector<double> extra_temps = {300.0, 3000.0};     // additional T sweep points
};

static int run(const Config& cfg) {
    fs::create_directories(cfg.output_dir);
    fs::create_directories(cfg.output_dir + "/elements");

    const std::string ts = timestamp_now();

    // Banner
    std::cout << ansi::BOLD;
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VSEPR-SIM Nuclear Core Runner — Z=2 (He) through Z=102 (No)   ║\n";
    std::cout << "║  gas2 three-EOS + kinetic + nuclear encoding                   ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  T = " << std::setw(8) << std::left << cfg.T_K
              << " K    P = " << std::setw(6) << cfg.P_atm << " atm"
              << "                           ║\n";
    std::cout << "║  Output: " << std::setw(55) << std::left << cfg.output_dir << "║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << ansi::RESET << "\n";

    auto t_start = std::chrono::steady_clock::now();

    std::vector<ElementResult> results;
    results.reserve(101);

    ExcelXMLWriter excel;
    excel.add_header();

    // CSV header
    std::string csv_path = cfg.output_dir + "/summary.csv";
    {
        std::ofstream csv(csv_path);
        csv << "Z,Symbol,Name,Category,DecayMode,MolarMass_g,Melting_K,Boiling_K,"
               "T_debye_K,Atomic_radius_pm,Electronegativity,Electron_affinity_eV,"
               "Resistivity_nOhm_m,"
               "Binding_MeV,Fissility,Ed_eV,Ionisation_eV,"
               "Sigma_th_barn,Mass_excess_keV,Sn_keV,"
               "Fissile,Fertile,HalfLife_s,"
               "T_K,P_atm,V_ideal_L,V_vdw_L,V_rk_L,Z_ideal,Z_vdw,Z_rk,"
               "Cv_Jmol,Cp_Jmol,gamma,v_rms_ms,v_mean_ms,v_mp_ms,c_sound_ms,MFP_m,"
               "KE_trans_J,KE_trans_Eh,"
               "S_ST_Jmol,H_Jmol,G_Jmol,A_Jmol,"
               "Lambda_m,nLambda3,"
               "ThermDiff_m2s,Lindemann_x1e3,Frenkel_eV,ReducedMass_H_amu,"
               "MaxRecoil_H_eV,Phase_at_T,AlphaRecoil_MeV,WignerSeitz_pm\n";
    }

    // JSON array start
    std::string json_path = cfg.output_dir + "/data.json";
    std::ofstream json_out;
    if (cfg.json) {
        json_out.open(json_path);
        if (json_out) json_out << "[\n";
    }

    // Main sweep Z=2..102
    bool json_first = true;
    for (int Z = 2; Z <= 102; ++Z) {
        const NuclearSpecies* ns = nuclear_species_ptr(Z);
        if (!ns) continue;

        auto ga_opt = analyze_element(Z, cfg.T_K, cfg.P_atm);
        if (!ga_opt) continue;

        ElementResult er{Z, std::move(*ga_opt), ns, {}};
        er.analytics = compute_analytics(er);
        const auto& ga  = er.gas;
        const auto& an  = er.analytics;

        // Progress line (enhanced)
        if (!cfg.quiet) {
            const char* col = category_color(ns->category);
            std::string phase_tag =
                an.is_gas_at_STP ? "gas" : an.above_boil ? "vap" : an.above_melt ? "liq" : "sol";
            std::cout << ansi::DIM << "  [Z=" << std::setw(3) << Z << "] "
                      << ansi::RESET
                      << col << std::setw(3) << std::left << ns->symbol << ansi::RESET
                      << " " << std::setw(16) << std::left << ns->name
                      << "  " << std::setw(17) << std::left
                              << nuclear_phase_category_name(ns->category)
                      << "  M="   << std::setw(9) << fmt_d(ns->molar_mass_g, 3)
                      << "  v_rms=" << std::setw(7) << fmt_d(ga.v_rms, 0) << " m/s"
                      << "  BE=" << std::setw(7) << fmt_d(ns->binding_energy_MeV, 4) << " MeV"
                      << "  G=" << std::setw(11) << fmt_sci(an.G_Jmol) << " J/mol"
                      << "  [" << phase_tag << "]"
                      << (ns->fissile ? (std::string(ansi::YELLOW) + "  [FISSILE]" + ansi::RESET) : "")
                      << (ns->fertile ? (std::string(ansi::GREEN)  + "  [FERTILE]" + ansi::RESET) : "")
                      << (an.nLambda3 > 1e-3 ? (std::string(ansi::CYAN) + "  [QM]" + ansi::RESET) : "")
                      << "\n";
        }
        {
            std::ostringstream md_path;
            md_path << cfg.output_dir << "/elements/Z-"
                    << std::setfill('0') << std::setw(3) << Z
                    << "-" << ns->symbol << ".md";
            std::ofstream mdf(md_path.str());
            if (mdf) mdf << element_markdown(er);
        }

        // Append to CSV
        {
            std::ofstream csv(csv_path, std::ios::app);
            if (csv) {
                std::string phase = an.is_gas_at_STP ? "gas" : an.above_boil ? "vapour" : an.above_melt ? "liquid" : "solid";
                csv << Z << "," << ns->symbol << "," << ns->name << ","
                    << nuclear_phase_category_name(ns->category) << ","
                    << decay_mode_name(ns->decay_mode) << ","
                    << fmt_d(ns->molar_mass_g, 4) << ","
                    << fmt_d(ns->melting_point_K, 1) << ","
                    << fmt_d(ns->boiling_point_K, 1) << ","
                    << fmt_d(ns->T_debye_K, 0) << ","
                    << fmt_d(ns->atomic_radius_pm, 0) << ","
                    << fmt_d(ns->electronegativity, 3) << ","
                    << fmt_d(ns->electron_affinity_eV, 4) << ","
                    << fmt_d(ns->resistivity_nOhm_m, 2) << ","
                    << fmt_d(ns->binding_energy_MeV, 5) << ","
                    << fmt_d(ns->fissility, 4) << ","
                    << fmt_d(ns->Ed_eV, 1) << ","
                    << fmt_d(ns->ionisation_eV, 4) << ","
                    << fmt_d(ns->sigma_thermal_b, 4) << ","
                    << fmt_d(ns->mass_excess_keV, 2) << ","
                    << fmt_d(ns->Sn_keV, 2) << ","
                    << (ns->fissile ? "1" : "0") << ","
                    << (ns->fertile ? "1" : "0") << ","
                    << fmt_sci(ns->half_life_s) << ","
                    << fmt_d(ga.T_K, 1) << ","
                    << fmt_d(ga.P_Pa / atm_to_Pa, 3) << ","
                    << fmt_d(ga.eos_ideal.V_L(), 5) << ","
                    << fmt_d(ga.eos_vdw.V_L(), 5) << ","
                    << fmt_d(ga.eos_rk.V_L(), 5) << ","
                    << fmt_d(ga.eos_ideal.Z, 5) << ","
                    << fmt_d(ga.eos_vdw.Z, 5) << ","
                    << fmt_d(ga.eos_rk.Z, 5) << ","
                    << fmt_d(ga.Cv_calc, 4) << ","
                    << fmt_d(ga.Cp_calc, 4) << ","
                    << fmt_d(ga.gamma_calc, 5) << ","
                    << fmt_d(ga.v_rms, 2) << ","
                    << fmt_d(ga.v_mean, 2) << ","
                    << fmt_d(ga.v_mp, 2) << ","
                    << fmt_d(ga.c_sound, 2) << ","
                    << fmt_sci(ga.mean_free_path_m) << ","
                    << fmt_sci(ga.ke_translational) << ","
                    << fmt_sci(ga.ke_translational_Eh) << ","
                    << fmt_d(an.S_ST_Jmol, 3) << ","
                    << fmt_sci(an.H_Jmol) << ","
                    << fmt_sci(an.G_Jmol) << ","
                    << fmt_sci(an.A_Jmol) << ","
                    << fmt_sci(an.Lambda_m) << ","
                    << fmt_sci(an.nLambda3) << ","
                    << (an.alpha_th_m2s > 0 ? fmt_sci(an.alpha_th_m2s) : "0") << ","
                    << (an.lindemann > 0 ? fmt_d(an.lindemann, 4) : "0") << ","
                    << fmt_d(an.frenkel_eV, 2) << ","
                    << fmt_d(an.reduced_mass_H, 5) << ","
                    << fmt_sci(an.max_recoil_H_eV) << ","
                    << phase << ","
                    << fmt_d(an.alpha_recoil_MeV, 4) << ","
                    << fmt_d(an.wigner_seitz_pm, 1) << "\n";
            }
        }

        // JSON record
        if (cfg.json && json_out) {
            std::string phase = an.is_gas_at_STP ? "gas" : an.above_boil ? "vapour" : an.above_melt ? "liquid" : "solid";
            if (!json_first) json_out << ",\n";
            json_first = false;
            json_out << "  {\n";
            json_out << "    \"Z\": " << Z << ",\n";
            json_out << "    \"symbol\": \"" << ns->symbol << "\",\n";
            json_out << "    \"name\": \"" << ns->name << "\",\n";
            json_out << "    \"isotope\": \"" << ns->isotope_label << "\",\n";
            json_out << "    \"category\": \"" << nuclear_phase_category_name(ns->category) << "\",\n";
            json_out << "    \"decay_mode\": \"" << decay_mode_name(ns->decay_mode) << "\",\n";
            json_out << "    \"crystal_phase\": \"" << ns->crystal_phase << "\",\n";
            json_out << "    \"molar_mass_g\": " << fmt_d(ns->molar_mass_g, 5) << ",\n";
            json_out << "    \"melting_K\": " << fmt_d(ns->melting_point_K, 1) << ",\n";
            json_out << "    \"boiling_K\": " << fmt_d(ns->boiling_point_K, 1) << ",\n";
            json_out << "    \"T_debye_K\": " << fmt_d(ns->T_debye_K, 0) << ",\n";
            json_out << "    \"atomic_radius_pm\": " << fmt_d(ns->atomic_radius_pm, 0) << ",\n";
            json_out << "    \"electronegativity\": " << fmt_d(ns->electronegativity, 3) << ",\n";
            json_out << "    \"electron_affinity_eV\": " << fmt_d(ns->electron_affinity_eV, 4) << ",\n";
            json_out << "    \"resistivity_nOhm_m\": " << fmt_d(ns->resistivity_nOhm_m, 3) << ",\n";
            json_out << "    \"k_thermal_W_mK\": " << fmt_d(ns->k_thermal_W_mK, 4) << ",\n";
            json_out << "    \"Cp_solid_J_kgK\": " << fmt_d(ns->Cp_solid_J_kgK, 2) << ",\n";
            json_out << "    \"binding_energy_MeV\": " << fmt_d(ns->binding_energy_MeV, 5) << ",\n";
            json_out << "    \"fissility\": " << fmt_d(ns->fissility, 4) << ",\n";
            json_out << "    \"Ed_eV\": " << fmt_d(ns->Ed_eV, 1) << ",\n";
            json_out << "    \"ionisation_eV\": " << fmt_d(ns->ionisation_eV, 4) << ",\n";
            json_out << "    \"sigma_thermal_b\": " << fmt_d(ns->sigma_thermal_b, 4) << ",\n";
            json_out << "    \"mass_excess_keV\": " << fmt_d(ns->mass_excess_keV, 2) << ",\n";
            json_out << "    \"Sn_keV\": " << fmt_d(ns->Sn_keV, 2) << ",\n";
            json_out << "    \"fissile\": " << (ns->fissile ? "true" : "false") << ",\n";
            json_out << "    \"fertile\": " << (ns->fertile ? "true" : "false") << ",\n";
            json_out << "    \"half_life_s\": " << fmt_sci(ns->half_life_s) << ",\n";
            json_out << "    \"T_analysis_K\": " << fmt_d(ga.T_K, 1) << ",\n";
            json_out << "    \"P_atm\": " << fmt_d(ga.P_Pa / atm_to_Pa, 3) << ",\n";
            json_out << "    \"V_ideal_L\": " << fmt_d(ga.eos_ideal.V_L(), 5) << ",\n";
            json_out << "    \"V_vdw_L\": "  << fmt_d(ga.eos_vdw.V_L(), 5) << ",\n";
            json_out << "    \"V_rk_L\": "   << fmt_d(ga.eos_rk.V_L(), 5) << ",\n";
            json_out << "    \"Z_vdw\": "    << fmt_d(ga.eos_vdw.Z, 5) << ",\n";
            json_out << "    \"Z_rk\": "     << fmt_d(ga.eos_rk.Z, 5) << ",\n";
            json_out << "    \"Cv_Jmol\": " << fmt_d(ga.Cv_calc, 4) << ",\n";
            json_out << "    \"Cp_Jmol\": " << fmt_d(ga.Cp_calc, 4) << ",\n";
            json_out << "    \"gamma\": " << fmt_d(ga.gamma_calc, 5) << ",\n";
            json_out << "    \"v_rms_ms\": " << fmt_d(ga.v_rms, 2) << ",\n";
            json_out << "    \"v_mean_ms\": " << fmt_d(ga.v_mean, 2) << ",\n";
            json_out << "    \"v_mp_ms\": " << fmt_d(ga.v_mp, 2) << ",\n";
            json_out << "    \"c_sound_ms\": " << fmt_d(ga.c_sound, 2) << ",\n";
            json_out << "    \"MFP_m\": " << fmt_sci(ga.mean_free_path_m) << ",\n";
            json_out << "    \"KE_trans_J\": " << fmt_sci(ga.ke_translational) << ",\n";
            json_out << "    \"KE_trans_Eh\": " << fmt_sci(ga.ke_translational_Eh) << ",\n";
            json_out << "    \"S_ST_Jmol\": " << fmt_d(an.S_ST_Jmol, 3) << ",\n";
            json_out << "    \"H_Jmol\": " << fmt_sci(an.H_Jmol) << ",\n";
            json_out << "    \"G_Jmol\": " << fmt_sci(an.G_Jmol) << ",\n";
            json_out << "    \"A_Jmol\": " << fmt_sci(an.A_Jmol) << ",\n";
            json_out << "    \"Lambda_m\": " << fmt_sci(an.Lambda_m) << ",\n";
            json_out << "    \"nLambda3\": " << fmt_sci(an.nLambda3) << ",\n";
            json_out << "    \"thermal_diffusivity_m2s\": " << fmt_sci(an.alpha_th_m2s) << ",\n";
            json_out << "    \"lindemann_x1e3\": " << fmt_d(an.lindemann, 4) << ",\n";
            json_out << "    \"frenkel_eV\": " << fmt_d(an.frenkel_eV, 2) << ",\n";
            json_out << "    \"reduced_mass_H_amu\": " << fmt_d(an.reduced_mass_H, 5) << ",\n";
            json_out << "    \"max_recoil_H_eV\": " << fmt_sci(an.max_recoil_H_eV) << ",\n";
            json_out << "    \"alpha_recoil_MeV\": " << fmt_d(an.alpha_recoil_MeV, 4) << ",\n";
            json_out << "    \"wigner_seitz_pm\": " << fmt_d(an.wigner_seitz_pm, 1) << ",\n";
            json_out << "    \"phase_at_T\": \"" << phase << "\"\n";
            json_out << "  }";
        }

        excel.add_element(er);
        results.push_back(std::move(er));
    }

    // Close JSON array
    if (cfg.json && json_out) {
        json_out << "\n]\n";
        json_out.close();
    }

    auto t_end = std::chrono::steady_clock::now();
    double elapsed_s = std::chrono::duration<double>(t_end - t_start).count();

    // Write LaTeX master
    {
        std::string tex_path = cfg.output_dir + "/master_report.tex";
        std::ofstream tex(tex_path);
        if (tex) {
            tex << LaTeXWriter::preamble(ts, static_cast<int>(results.size()));
            tex << LaTeXWriter::summary_table(results);
            tex << "\\section{Per-Element Detailed Sections}\n\n";
            tex << "\\newpage\n\n";
            for (const auto& er : results) {
                tex << LaTeXWriter::element_section(er);
            }
            tex << LaTeXWriter::footer();
            std::cout << "\n  " << ansi::GREEN << "LaTeX : " << ansi::RESET << tex_path << "\n";
        }
    }

    // Write Excel XML
    {
        std::string xml_path = cfg.output_dir + "/data.xml";
        std::ofstream xml(xml_path);
        if (xml) {
            xml << excel.to_xml();
            std::cout << "  " << ansi::GREEN << "Excel : " << ansi::RESET << xml_path << "\n";
        }
    }

    // Write JSON
    if (cfg.json) {
        std::cout << "  " << ansi::GREEN << "JSON  : " << ansi::RESET << json_path << "\n";
    }

    // =========================================================================
    // Cross-element analytics: ranking tables
    // =========================================================================
    {
        std::string rank_path = cfg.output_dir + "/cross_element_rankings.md";
        std::ofstream rank(rank_path);
        if (rank) {
            rank << "# Cross-Element Rankings — Nuclear Core Z=2..102\n\n";
            rank << "Generated: " << ts << "  \n";
            rank << "Analysis T = " << fmt_d(cfg.T_K, 0) << " K, P = " << fmt_d(cfg.P_atm, 2) << " atm\n\n";

            // Helper lambda: top-N ranked by some field
            auto top_n = [&](const std::string& title,
                              std::function<double(const ElementResult&)> fn,
                              int N = 10, bool ascending = false) {
                std::vector<std::pair<double, const ElementResult*>> ranked;
                ranked.reserve(results.size());
                for (const auto& er : results)
                    ranked.push_back({fn(er), &er});
                std::sort(ranked.begin(), ranked.end(),
                    [ascending](const auto& a, const auto& b){
                        return ascending ? a.first < b.first : a.first > b.first;
                    });
                rank << "## " << title << "\n\n";
                rank << "| Rank | Z | Symbol | Name | Value |\n";
                rank << "|------|---|--------|------|-------|\n";
                int lim = std::min(N, static_cast<int>(ranked.size()));
                for (int i = 0; i < lim; ++i) {
                    const auto& er = *ranked[i].second;
                    rank << "| " << (i+1) << " | " << er.Z << " | "
                         << er.nuclear->symbol << " | " << er.nuclear->name
                         << " | " << fmt_d(ranked[i].first, 5) << " |\n";
                }
                rank << "\n";
            };

            top_n("Highest Binding Energy per Nucleon (MeV)",
                  [](const ElementResult& er){ return er.nuclear->binding_energy_MeV; });
            top_n("Lowest Binding Energy per Nucleon (MeV)",
                  [](const ElementResult& er){ return er.nuclear->binding_energy_MeV; }, 10, true);
            top_n("Highest Fissility Z²/A",
                  [](const ElementResult& er){ return er.nuclear->fissility; });
            top_n("Highest Thermal Neutron Cross-Section (barn)",
                  [](const ElementResult& er){ return er.nuclear->sigma_thermal_b; });
            top_n("Highest Displacement Energy Ed (eV)",
                  [](const ElementResult& er){ return er.nuclear->Ed_eV; });
            top_n("Highest v_rms at analysis T (m/s)",
                  [](const ElementResult& er){ return er.gas.v_rms; });
            top_n("Lowest v_rms at analysis T (m/s)",
                  [](const ElementResult& er){ return er.gas.v_rms; }, 10, true);
            top_n("Most Negative Gibbs Free Energy G (J/mol)",
                  [](const ElementResult& er){ return -er.analytics.G_Jmol; });
            top_n("Highest Sackur-Tetrode Entropy S (J/mol/K)",
                  [](const ElementResult& er){ return er.analytics.S_ST_Jmol; });
            top_n("Highest Melting Point (K)",
                  [](const ElementResult& er){ return er.nuclear->melting_point_K; });
            top_n("Highest Thermal Conductivity k (W/m/K)",
                  [](const ElementResult& er){ return er.nuclear->k_thermal_W_mK; });
            top_n("Highest Debye Temperature (K)",
                  [](const ElementResult& er){ return er.nuclear->T_debye_K; });
            top_n("Highest Electronegativity (Pauling)",
                  [](const ElementResult& er){ return er.nuclear->electronegativity; });
            top_n("Highest Electron Affinity (eV)",
                  [](const ElementResult& er){ return er.nuclear->electron_affinity_eV; });
            top_n("Highest First Ionisation Energy (eV)",
                  [](const ElementResult& er){ return er.nuclear->ionisation_eV; });
            top_n("Largest Atomic Radius (pm)",
                  [](const ElementResult& er){ return er.nuclear->atomic_radius_pm; });
            top_n("Largest Neutron Separation Energy Sn (keV)",
                  [](const ElementResult& er){ return er.nuclear->Sn_keV; });
            top_n("Most Negative Mass Excess Δ (keV)",
                  [](const ElementResult& er){ return -er.nuclear->mass_excess_keV; });
            top_n("Highest Thermal Diffusivity (m²/s)",
                  [](const ElementResult& er){ return er.analytics.alpha_th_m2s; });
            top_n("Largest Frenkel Pair Energy (eV)",
                  [](const ElementResult& er){ return er.analytics.frenkel_eV; });

            std::cout << "  " << ansi::GREEN << "Rankings: " << ansi::RESET << rank_path << "\n";
        }
    }

    // =========================================================================
    // Multi-temperature sweep (extra_temps) — summary CSV only
    // =========================================================================
    if (!cfg.extra_temps.empty()) {
        std::string mt_path = cfg.output_dir + "/multitemp_sweep.csv";
        std::ofstream mt(mt_path);
        if (mt) {
            mt << "T_K,Z,Symbol,Name,v_rms_ms,c_sound_ms,S_ST_Jmol,G_Jmol,Lambda_m,nLambda3,Phase\n";
            for (double T_extra : cfg.extra_temps) {
                for (int Z = 2; Z <= 102; ++Z) {
                    const NuclearSpecies* ns = nuclear_species_ptr(Z);
                    if (!ns) continue;
                    auto ga_opt = analyze_element(Z, T_extra, cfg.P_atm);
                    if (!ga_opt) continue;
                    ElementResult er2{Z, std::move(*ga_opt), ns, {}};
                    er2.analytics = compute_analytics(er2);
                    const auto& an2 = er2.analytics;
                    std::string phase = an2.is_gas_at_STP ? "gas" : an2.above_boil ? "vapour" : an2.above_melt ? "liquid" : "solid";
                    mt << fmt_d(T_extra, 0) << ","
                       << Z << "," << ns->symbol << "," << ns->name << ","
                       << fmt_d(er2.gas.v_rms, 2) << ","
                       << fmt_d(er2.gas.c_sound, 2) << ","
                       << fmt_d(an2.S_ST_Jmol, 3) << ","
                       << fmt_sci(an2.G_Jmol) << ","
                       << fmt_sci(an2.Lambda_m) << ","
                       << fmt_sci(an2.nLambda3) << ","
                       << phase << "\n";
                }
            }
            std::cout << "  " << ansi::GREEN << "Multi-T : " << ansi::RESET << mt_path
                      << "  (" << cfg.extra_temps.size() << " extra temperatures)\n";
        }
    }

    // Summary
    int n_fissile = 0, n_fertile = 0, n_actinide = 0, n_lanthanide = 0, n_noble = 0;
    int n_above_boil = 0, n_above_melt = 0, n_qm = 0;
    double max_vrms = 0.0, min_vrms = 1e12;
    double max_BE = 0.0, min_BE = 99.0;
    double max_sigma = 0.0;
    const ElementResult* er_max_vrms  = nullptr;
    const ElementResult* er_min_vrms  = nullptr;
    const ElementResult* er_max_BE    = nullptr;
    const ElementResult* er_max_sigma = nullptr;
    double sum_S = 0.0, sum_G = 0.0;
    for (const auto& er : results) {
        if (er.nuclear->fissile) ++n_fissile;
        if (er.nuclear->fertile) ++n_fertile;
        if (er.nuclear->category == NuclearPhaseCategory::Actinide)   ++n_actinide;
        if (er.nuclear->category == NuclearPhaseCategory::Lanthanide) ++n_lanthanide;
        if (er.nuclear->category == NuclearPhaseCategory::NobleGas)   ++n_noble;
        if (er.analytics.above_boil)  ++n_above_boil;
        if (er.analytics.above_melt && !er.analytics.above_boil) ++n_above_melt;
        if (er.analytics.nLambda3 > 1e-3) ++n_qm;
        if (er.gas.v_rms > max_vrms) { max_vrms = er.gas.v_rms; er_max_vrms = &er; }
        if (er.gas.v_rms < min_vrms) { min_vrms = er.gas.v_rms; er_min_vrms = &er; }
        if (er.nuclear->binding_energy_MeV > max_BE) { max_BE = er.nuclear->binding_energy_MeV; er_max_BE = &er; }
        if (er.nuclear->binding_energy_MeV < min_BE) { min_BE = er.nuclear->binding_energy_MeV; }
        if (er.nuclear->sigma_thermal_b > max_sigma) { max_sigma = er.nuclear->sigma_thermal_b; er_max_sigma = &er; }
        sum_S += er.analytics.S_ST_Jmol;
        sum_G += er.analytics.G_Jmol;
    }

    std::cout << "\n" << ansi::BOLD;
    std::cout << "  ══════════════════════════════════════════════════════════════════════\n";
    std::cout << "   Nuclear Core Z=2..102 — Run Complete\n";
    std::cout << "   Elements swept     :  " << results.size() << " (Z=2 He → Z=102 No)\n";
    std::cout << "  ──────────────────────────────────────────────────────────────────────\n";
    std::cout << "   Actinides          :  " << n_actinide << "\n";
    std::cout << "   Lanthanides        :  " << n_lanthanide << "\n";
    std::cout << "   Noble gases        :  " << n_noble << "\n";
    std::cout << "   Fissile species    :  " << n_fissile << "  (";
    for (const auto& er : results)
        if (er.nuclear->fissile) std::cout << er.nuclear->symbol << " ";
    std::cout << ")\n";
    std::cout << "   Fertile species    :  " << n_fertile << "  (";
    for (const auto& er : results)
        if (er.nuclear->fertile) std::cout << er.nuclear->symbol << " ";
    std::cout << ")\n";
    std::cout << "  ──────────────────────────────────────────────────────────────────────\n";
    std::cout << "   Phase at T=" << fmt_d(cfg.T_K, 0) << " K:\n";
    std::cout << "     Above boiling point (vapour) : " << n_above_boil << " elements\n";
    std::cout << "     Above melting (liquid)        : " << n_above_melt << " elements\n";
    std::cout << "     Quantum indicator (nΛ³>1e-3)  : " << n_qm << " elements\n";
    std::cout << "  ──────────────────────────────────────────────────────────────────────\n";
    std::cout << "   v_rms range        :  " << fmt_d(min_vrms, 0) << " – "
              << fmt_d(max_vrms, 0) << " m/s\n";
    if (er_max_vrms) std::cout << "     Fastest  : " << er_max_vrms->nuclear->symbol
                               << " (Z=" << er_max_vrms->Z << ")  M="
                               << fmt_d(er_max_vrms->nuclear->molar_mass_g,3) << " g/mol\n";
    if (er_min_vrms) std::cout << "     Slowest  : " << er_min_vrms->nuclear->symbol
                               << " (Z=" << er_min_vrms->Z << ")  M="
                               << fmt_d(er_min_vrms->nuclear->molar_mass_g,3) << " g/mol\n";
    std::cout << "   BE/A range         :  " << fmt_d(min_BE, 4) << " – "
              << fmt_d(max_BE, 4) << " MeV";
    if (er_max_BE) std::cout << "  (peak: " << er_max_BE->nuclear->symbol << " Z=" << er_max_BE->Z << ")";
    std::cout << "\n";
    if (er_max_sigma) std::cout << "   Highest σ_th       :  " << fmt_d(max_sigma, 1)
                                << " b  (" << er_max_sigma->nuclear->symbol
                                << ", Z=" << er_max_sigma->Z << ")\n";
    std::cout << "   Mean S_ST          :  " << fmt_d(sum_S / results.size(), 2)
              << " J/(mol·K)\n";
    std::cout << "   Mean G             :  " << fmt_sci(sum_G / results.size())
              << " J/mol\n";
    std::cout << "  ──────────────────────────────────────────────────────────────────────\n";
    std::cout << "   Wall-clock         :  " << fmt_d(elapsed_s, 3) << " s\n";
    std::cout << "   Rate               :  "
              << fmt_d(static_cast<double>(results.size()) / elapsed_s, 0) << " elements/s\n";
    std::cout << "   Output             :  " << cfg.output_dir << "/\n";
    std::cout << "     master_report.tex          — compilable LaTeX (extended)\n";
    std::cout << "     data.xml                   — SpreadsheetML (Excel, 57 columns)\n";
    std::cout << "     data.json                  — JSON array (all fields)\n";
    std::cout << "     summary.csv                — flat CSV (extended)\n";
    std::cout << "     cross_element_rankings.md  — 20 ranking tables\n";
    std::cout << "     multitemp_sweep.csv         — " << (cfg.extra_temps.size())
              << " extra temperature points\n";
    std::cout << "     elements/Z-NNN-SYM.md      — per-element Markdown (enhanced)\n";
    std::cout << "  ══════════════════════════════════════════════════════════════════════\n";
    std::cout << ansi::RESET << "\n";

    return 0;
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
    Config cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "nuclear-core-z2-102 — VSEPR-SIM nuclear species sweep Z=2..102\n\n";
            std::cout << "Options:\n";
            std::cout << "  --T N          Primary analysis temperature K (default: 1000)\n";
            std::cout << "  --P N          Pressure atm (default: 1.0)\n";
            std::cout << "  --out DIR      Output directory (default: reports/nuclear_core_z2_102)\n";
            std::cout << "  --temps T1,T2  Extra temperatures for multi-T sweep (default: 300,3000)\n";
            std::cout << "  --no-json      Suppress JSON output (default: on)\n";
            std::cout << "  --quiet        Suppress per-element progress\n";
            std::cout << "  --help         This help\n\n";
            std::cout << "Outputs:\n";
            std::cout << "  master_report.tex          — compilable LaTeX (extended tables)\n";
            std::cout << "  data.xml                   — SpreadsheetML (Excel, 57+ columns)\n";
            std::cout << "  data.json                  — JSON array (all fields per element)\n";
            std::cout << "  summary.csv                — flat CSV (extended)\n";
            std::cout << "  cross_element_rankings.md  — 20 cross-element ranking tables\n";
            std::cout << "  multitemp_sweep.csv        — multi-temperature kinetic sweep\n";
            std::cout << "  elements/Z-NNN-SYM.md      — per-element Markdown (enhanced)\n";
            return 0;
        }
        else if (arg == "--T" && i + 1 < argc) {
            cfg.T_K = std::stod(argv[++i]);
        }
        else if (arg == "--P" && i + 1 < argc) {
            cfg.P_atm = std::stod(argv[++i]);
        }
        else if (arg == "--out" && i + 1 < argc) {
            cfg.output_dir = argv[++i];
        }
        else if (arg == "--temps" && i + 1 < argc) {
            cfg.extra_temps.clear();
            std::string tlist = argv[++i];
            std::istringstream ss(tlist);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                try { cfg.extra_temps.push_back(std::stod(tok)); } catch (...) {}
            }
        }
        else if (arg == "--no-json") {
            cfg.json = false;
        }
        else if (arg == "--quiet") {
            cfg.quiet = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\nRun with --help for usage.\n";
            return 1;
        }
    }

    return run(cfg);
}
