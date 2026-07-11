/**
 * gas2_engine.cpp
 * ---------------
 * Implementation of the gas2 unified engine, thermal report, and CLI dispatch.
 */

#include "gas2/gas2_engine.hpp"
#include "gas2/gas2_tui.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace vsepr {
namespace gas2 {

// ============================================================================
// ThermalReport::format()
// ============================================================================

std::string ThermalReport::format() const {
    std::ostringstream ss;
    ss << std::fixed;
    ss << "┌─ Thermal Properties: " << formula << " at " << std::setprecision(1) << T_K << " K\n";
    ss << "│\n";
    ss << "│  Degrees of freedom:\n";
    ss << "│    translational: " << dof.translational << "\n";
    ss << "│    rotational:    " << dof.rotational << "\n";
    ss << "│    vibrational:   " << dof.vibrational << " (2 per mode)\n";
    ss << "│    classical f:   " << dof.total_classical << " (low T, vib frozen)\n";
    ss << "│    full f:        " << dof.total_full << " (high T limit)\n";
    ss << "│\n";
    ss << "│  Heat capacity (from DOF, classical):\n";
    ss << std::setprecision(3);
    ss << "│    Cv = " << Cv_calc << " J/(mol·K)   [tabulated: " << Cv_tabulated << "]\n";
    ss << "│    Cp = " << Cp_calc << " J/(mol·K)   [tabulated: " << Cp_tabulated << "]\n";
    ss << "│    γ  = " << gamma_calc << "           [tabulated: " << gamma_tabulated << "]\n";
    ss << "│\n";
    ss << "│  Sound speed:    " << std::setprecision(1) << c_sound << " m/s\n";
    if (mu_JT != 0.0) {
        ss << "│  Joule-Thomson:  " << std::setprecision(4) << (mu_JT * 1e5) << " K/bar\n";
        ss << "│  T_inversion:    " << std::setprecision(0) << T_inversion << " K\n";
    }
    ss << "└\n";
    return ss.str();
}

// ============================================================================
// analyze()
// ============================================================================

Gas2Analysis analyze(const std::string& formula, double T_K, double P_atm,
                     double n_mol, uint64_t seed) {
    Gas2Analysis a{};
    a.formula = formula;
    a.T_K = T_K;
    a.P_Pa = P_atm * atm_to_Pa;
    a.n_mol = n_mol;
    a.seed = seed;

    // Species lookup (molecular DB first, then monatomic nuclear fallback)
    a.species = find_species_or_monatomic(formula, a.monatomic_storage);
    // Nuclear species reference — non-null when formula is an element symbol Z=2..102
    a.nuclear_species_ref = nullptr;
    for (int Z = 2; Z <= 102; ++Z) {
        const NuclearSpecies* ns = nuclear_species_ptr(Z);
        if (ns && std::string(ns->symbol) == formula) {
            a.nuclear_species_ref = ns;
            break;
        }
    }
    double M_kg = a.species ? a.species->molar_mass_kg() : 0.028; // N2 fallback
    double d_m  = a.species ? a.species->d_kinetic_m()  : 3.64e-10;

    // EOS — all three
    a.eos_ideal = ideal_gas(n_mol, T_K, a.P_Pa);

    if (a.species) {
        a.eos_vdw = vdw_solve_volume(n_mol, T_K, a.P_Pa,
                                      a.species->vdw_a, a.species->vdw_b);
        auto rk = rk_params_from_critical(a.species->Tc_K,
                                           a.species->Pc_atm * atm_to_Pa);
        a.eos_rk = rk_solve_volume(n_mol, T_K, a.P_Pa, rk.a_RK, rk.b_RK);
    } else {
        a.eos_vdw = a.eos_ideal;
        a.eos_vdw.type = EOSType::VanDerWaals;
        a.eos_rk = a.eos_ideal;
        a.eos_rk.type = EOSType::RedlichKwong;
    }

    // DOF
    int n_atoms = a.species ? a.species->n_atoms : 2;
    bool linear = (n_atoms <= 2);
    a.dof = compute_dof(n_atoms, linear);

    // Kinetic theory
    a.v_rms  = rms_speed(T_K, M_kg);
    a.v_mean = mean_speed(T_K, M_kg);
    a.v_mp   = most_probable_speed(T_K, M_kg);
    a.ke_translational = avg_translational_ke(T_K);
    a.ke_total = avg_total_ke(T_K, a.dof.total_classical);
    a.ke_translational_Eh = avg_translational_ke_Eh(T_K);
    a.ke_total_Eh = avg_total_ke_Eh(T_K, a.dof.total_classical);
    a.mean_free_path_m = mean_free_path(T_K, a.P_Pa, d_m);
    a.collision_freq = collision_frequency(T_K, a.P_Pa, M_kg, d_m);
    a.viscosity = viscosity_hard_sphere(T_K, M_kg, d_m);
    a.diffusion = self_diffusion(T_K, a.P_Pa, M_kg, d_m);

    // Heat
    a.Cv_calc = Cv_from_dof(a.dof.total_classical);
    a.Cp_calc = Cp_from_dof(a.dof.total_classical);
    a.gamma_calc = gamma_from_dof(a.dof.total_classical);
    a.c_sound = speed_of_sound(T_K, M_kg, a.gamma_calc);

    if (a.species) {
        a.mu_JT = joule_thomson_vdw(T_K, a.species->vdw_a, a.species->vdw_b,
                                     a.Cp_calc);
        a.T_inversion = inversion_temperature_vdw(a.species->vdw_a, a.species->vdw_b);
    } else {
        a.mu_JT = 0.0;
        a.T_inversion = 0.0;
    }

    return a;
}

// ============================================================================
// thermal_report()
// ============================================================================

ThermalReport thermal_report(const std::string& formula, double T_K) {
    ThermalReport r{};
    r.formula = formula;
    r.T_K = T_K;

    auto* sp = find_species(formula);
    int n_atoms = sp ? sp->n_atoms : 2;
    bool linear = (n_atoms <= 2);
    r.dof = compute_dof(n_atoms, linear);

    r.Cv_calc = Cv_from_dof(r.dof.total_classical);
    r.Cp_calc = Cp_from_dof(r.dof.total_classical);
    r.gamma_calc = gamma_from_dof(r.dof.total_classical);

    r.Cv_tabulated = sp ? sp->Cv_Jmol : r.Cv_calc;
    r.Cp_tabulated = sp ? sp->Cp_Jmol : r.Cp_calc;
    r.gamma_tabulated = sp ? sp->gamma : r.gamma_calc;

    double M_kg = sp ? sp->molar_mass_kg() : 0.028;
    r.c_sound = speed_of_sound(T_K, M_kg, r.gamma_calc);

    if (sp) {
        r.mu_JT = joule_thomson_vdw(T_K, sp->vdw_a, sp->vdw_b, r.Cp_calc);
        r.T_inversion = inversion_temperature_vdw(sp->vdw_a, sp->vdw_b);
    }

    return r;
}

// ============================================================================
// analyze_element() — atomic species sweep by Z
// ============================================================================

std::optional<Gas2Analysis> analyze_element(int Z, double T_K, double P_atm,
                                             double n_mol, uint64_t seed) {
    const NuclearSpecies* ns = nuclear_species_ptr(Z);
    if (!ns) return std::nullopt;
    return analyze(std::string(ns->symbol), T_K, P_atm, n_mol, seed);
}

std::string Gas2Analysis::format_full_report() const {
    std::ostringstream ss;
    ss << std::fixed;

    ss << "\n\033[1;38;5;213m"
       << "╔══════════════════════════════════════════════════════════════════════╗\n"
       << "║  ⚛  gas2 Full Analysis                                             ║\n"
       << "║     " << std::setw(65) << std::left << formula << "║\n"
       << "╚══════════════════════════════════════════════════════════════════════╝\n"
       << "\033[0m\n";

    // Conditions
    ss << "\033[38;5;75m┌─ Conditions\033[0m\n";
    ss << std::setprecision(2);
    ss << "\033[38;5;250m│\033[0m  T = \033[1;38;5;214m" << T_K << " K\033[0m,  P = "
       << (P_Pa / atm_to_Pa) << " atm,  n = " << n_mol << " mol\n";
    if (species) {
        ss << "\033[38;5;250m│\033[0m  \033[1;38;5;219m" << species->name << "\033[0m (" << species->formula << "),  M = "
           << std::setprecision(3) << species->molar_mass_g << " g/mol,  "
           << species->n_atoms << " atoms/molecule\n";
    }
    ss << "\033[38;5;250m│\033[0m\n";

    // EOS comparison with visual bars
    ss << "\033[38;5;75m┌─ Equations of State\033[0m\n";
    ss << std::setprecision(4);
    double v_max_eos = std::max({eos_ideal.V_L(), eos_vdw.V_L(), eos_rk.V_L()});
    if (v_max_eos <= 0) v_max_eos = 1.0;
    auto eos_bar = [&](double V, int color) -> std::string {
        int len = static_cast<int>(V / v_max_eos * 20);
        if (len < 1) len = 1;
        std::ostringstream b;
        b << "\033[38;5;" << color << "m";
        for (int i = 0; i < len; ++i) b << "█";
        b << "\033[0m";
        return b.str();
    };
    ss << "\033[38;5;250m│\033[0m  Ideal     " << eos_bar(eos_ideal.V_L(), 75) << " "
       << eos_ideal.V_L() << " L  Z=" << eos_ideal.Z << "\n";
    ss << "\033[38;5;250m│\033[0m  VdW       " << eos_bar(eos_vdw.V_L(), 156) << " "
       << eos_vdw.V_L() << " L  Z=" << eos_vdw.Z
       << "  (" << eos_vdw.iterations << " iter)\n";
    ss << "\033[38;5;250m│\033[0m  R-K       " << eos_bar(eos_rk.V_L(), 213) << " "
       << eos_rk.V_L() << " L  Z=" << eos_rk.Z
       << "  (" << eos_rk.iterations << " iter)\n";
    ss << "\033[38;5;250m│\033[0m\n";

    // Kinetic theory with speed comparison gauge
    ss << "\033[38;5;75m┌─ Kinetic Theory\033[0m\n";
    ss << std::setprecision(1);
    double v_scale = (v_rms > 0) ? v_rms : 1.0;
    auto speed_gauge = [&](double v, int color) -> std::string {
        int len = static_cast<int>(v / v_scale * 16);
        if (len < 1) len = 1;
        std::ostringstream b;
        b << "\033[38;5;" << color << "m";
        for (int i = 0; i < len; ++i) b << "━";
        b << "▸\033[0m";
        return b.str();
    };
    ss << "\033[38;5;250m│\033[0m  v_mp   = " << std::setw(8) << v_mp << " m/s  " << speed_gauge(v_mp, 226) << "\n";
    ss << "\033[38;5;250m│\033[0m  v_mean = " << std::setw(8) << v_mean << " m/s  " << speed_gauge(v_mean, 48) << "\n";
    ss << "\033[38;5;250m│\033[0m  v_rms  = " << std::setw(8) << v_rms << " m/s  " << speed_gauge(v_rms, 196) << "\n";
    ss << std::scientific << std::setprecision(4);
    ss << "\033[38;5;250m│\033[0m  KE_trans = \033[38;5;123m" << ke_translational << " J\033[0m   "
       << std::fixed << std::setprecision(6) << (ke_translational / eV_to_J) << " eV   "
       << std::scientific << std::setprecision(4) << "\033[1;38;5;214m" << ke_translational_Eh << " Eh\033[0m\n";
    ss << "\033[38;5;250m│\033[0m  KE_total = \033[38;5;123m" << ke_total << " J\033[0m   "
       << std::fixed << std::setprecision(6) << (ke_total / eV_to_J) << " eV   "
       << std::scientific << std::setprecision(4) << "\033[1;38;5;214m" << ke_total_Eh << " Eh\033[0m"
       << "  (f=" << dof.total_classical << ")\n";
    ss << std::fixed << std::setprecision(1);
    ss << "\033[38;5;250m│\033[0m  MFP     = " << (mean_free_path_m * 1e9) << " nm\n";
    ss << "\033[38;5;250m│\033[0m  z_coll  = " << std::setprecision(3) << (collision_freq * 1e-9) << " GHz\n";
    ss << "\033[38;5;250m│\033[0m\n";

    // Transport
    ss << "\033[38;5;75m┌─ Transport (Chapman-Enskog)\033[0m\n";
    ss << std::setprecision(2);
    ss << "\033[38;5;250m│\033[0m  η (viscosity)  = " << (viscosity * 1e6) << " μPa·s";
    if (species) {
        ss << "  \033[38;5;245m[tab: " << species->viscosity_uPas << "]\033[0m";
    }
    ss << "\n";
    ss << "\033[38;5;250m│\033[0m  D (diffusion)  = " << std::setprecision(4) << (diffusion * 1e4) << " cm²/s\n";
    ss << "\033[38;5;250m│\033[0m\n";

    // Heat with DOF visual
    ss << "\033[38;5;75m┌─ Thermal\033[0m\n";
    ss << std::setprecision(3);
    // Visual DOF breakdown
    ss << "\033[38;5;250m│\033[0m  DOF: ";
    for (int i = 0; i < dof.translational; ++i) ss << "\033[38;5;196m●\033[0m";
    ss << " trans  ";
    for (int i = 0; i < dof.rotational; ++i) ss << "\033[38;5;48m●\033[0m";
    ss << " rot  ";
    if (dof.vibrational > 0) {
        for (int i = 0; i < std::min(dof.vibrational, 6); ++i) ss << "\033[38;5;226m●\033[0m";
        if (dof.vibrational > 6) ss << "…";
        ss << " vib";
    }
    ss << "  = \033[1m" << dof.total_classical << "\033[0m classical";
    if (dof.total_full != dof.total_classical)
        ss << " (" << dof.total_full << " full)";
    ss << "\n";
    ss << "\033[38;5;250m│\033[0m  Cv = " << Cv_calc << " J/(mol·K)";
    if (species) ss << "  \033[38;5;245m[tab: " << species->Cv_Jmol << "]\033[0m";
    ss << "\n";
    ss << "\033[38;5;250m│\033[0m  Cp = " << Cp_calc << " J/(mol·K)";
    if (species) ss << "  \033[38;5;245m[tab: " << species->Cp_Jmol << "]\033[0m";
    ss << "\n";
    ss << "\033[38;5;250m│\033[0m  γ  = \033[1m" << gamma_calc << "\033[0m";
    if (species) ss << "  \033[38;5;245m[tab: " << species->gamma << "]\033[0m";
    ss << "\n";
    ss << std::setprecision(1);
    ss << "\033[38;5;250m│\033[0m  c_sound = " << c_sound << " m/s\n";
    if (species) {
        ss << std::setprecision(4);
        ss << "\033[38;5;250m│\033[0m  μ_JT    = " << (mu_JT * 1e5) << " K/bar\n";
        ss << std::setprecision(0);
        ss << "\033[38;5;250m│\033[0m  T_inv   = " << T_inversion << " K";
        if (T_K < T_inversion) ss << "  \033[38;5;75m❄ cooling on expansion\033[0m";
        else ss << "  \033[38;5;196m🔥 heating on expansion\033[0m";
        ss << "\n";
    }
    ss << "\033[38;5;250m└\033[0m\n";

    return ss.str();
}

std::string Gas2Analysis::format_compact() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << formula << " @ " << T_K << "K, " << (P_Pa / atm_to_Pa) << "atm: "
       << "V=" << std::setprecision(3) << eos_vdw.V_L() << "L "
       << "v_rms=" << std::setprecision(0) << v_rms << "m/s "
       << "Z=" << std::setprecision(4) << eos_vdw.Z;
    return ss.str();
}

std::string Gas2Analysis::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{";
    ss << "\"formula\":\"" << formula << "\",";
    ss << "\"T_K\":" << T_K << ",";
    ss << "\"P_atm\":" << (P_Pa / atm_to_Pa) << ",";
    ss << "\"n_mol\":" << n_mol << ",";
    ss << "\"V_ideal_L\":" << eos_ideal.V_L() << ",";
    ss << "\"V_vdw_L\":" << eos_vdw.V_L() << ",";
    ss << "\"V_rk_L\":" << eos_rk.V_L() << ",";
    ss << "\"Z_vdw\":" << eos_vdw.Z << ",";
    ss << "\"Z_rk\":" << eos_rk.Z << ",";
    ss << "\"v_rms\":" << v_rms << ",";
    ss << "\"v_mean\":" << v_mean << ",";
    ss << "\"mfp_nm\":" << (mean_free_path_m * 1e9) << ",";
    ss << "\"Cv\":" << Cv_calc << ",";
    ss << "\"Cp\":" << Cp_calc << ",";
    ss << "\"gamma\":" << gamma_calc << ",";
    ss << "\"c_sound\":" << c_sound << ",";
    ss << "\"ke_trans_J\":" << ke_translational << ",";
    ss << "\"ke_trans_eV\":" << (ke_translational / eV_to_J) << ",";
    ss << "\"ke_trans_Eh\":" << ke_translational_Eh << ",";
    ss << "\"ke_total_J\":" << ke_total << ",";
    ss << "\"ke_total_eV\":" << (ke_total / eV_to_J) << ",";
    ss << "\"ke_total_Eh\":" << ke_total_Eh;
    ss << "}";
    return ss.str();
}

// ============================================================================
// CLI dispatch
// ============================================================================

static void show_gas2_help() {
    std::cout << R"(
gas2 MODULE — Advanced Heat and Gas Analysis
═════════════════════════════════════════════

USAGE:
    vsepr gas2 <command> [options]

COMMANDS:
    analyze <FORMULA> [opts]   Full analysis (EOS + kinetic + thermal)
    thermal <FORMULA> [opts]   Thermal property report (DOF, Cp, Cv, γ)
    compare <FORMULA> [opts]   Compare Ideal vs VdW vs Redlich-Kwong
    phase   <FORMULA> [opts]   Phase equilibrium (A, G, μ, Maxwell construction)
    potentials <FORMULA> [opts] 8-channel potential decomposition + F[φ]
    sample  <FORMULA> [opts]   Maxwell-Boltzmann velocity sampling
    heatmap <FORMULA> [opts]   Heat maps + atom grid (pipe to gas2_heatmap.py)
    species [FORMULA]          Show species database (or one entry)
    monitor <FORMULA> [opts]   Start steady-state monitor (writes JSON)
    tui     [FORMULA] [opts]   Interactive terminal UI (species browser, live analysis, sweep)
    help                       Show this help

OPTIONS:
    -T, --temperature <K>    Temperature (default: 298.15 K)
    -P, --pressure <atm>     Pressure (default: 1.0 atm)
    -n, --moles <mol>        Amount (default: 1.0 mol)
    -N, --count <N>          Sample count for MB (default: 10000)
    --grid <N>               Grid resolution: 48, 64, or 256 (default: 64)
    --seed <S>               RNG seed (default: 42)
    --json                   Output as JSON
    --compact                Compact one-line output

SUPPORTED SPECIES:
    He, Ne, Ar, Kr, Xe       (noble)
    H2, N2, O2, Cl2          (diatomic)
    H2O, CO2, SO2, NH3, CH4  (polyatomic)

EXAMPLES:
    vsepr gas2 analyze Ar -T 300 -P 1.0
    vsepr gas2 thermal CO2 -T 500
    vsepr gas2 compare N2 -T 200 -P 50
    vsepr gas2 sample Xe -T 77 -N 10000
    vsepr gas2 heatmap Ar -T 300 -N 10000 --grid 64 | python tools/gas2_heatmap.py
    vsepr gas2 species
    vsepr gas2 species Ar

)";
}

int gas2_dispatch(int argc, char** argv) {
    if (argc < 3) {
        show_gas2_help();
        return 0;
    }

    std::string subcmd = argv[2];

    if (subcmd == "help" || subcmd == "--help" || subcmd == "-h") {
        show_gas2_help();
        return 0;
    }

    // ---- tui ----
    if (subcmd == "tui") {
        std::string tui_formula = (argc >= 4 && argv[3][0] != '-') ? argv[3] : "Ar";
        double tui_T = 298.15, tui_P = 1.0, tui_n = 1.0;
        for (int i = (argc >= 4 && argv[3][0] != '-') ? 4 : 3; i < argc; ++i) {
            std::string a = argv[i];
            if ((a == "-T" || a == "--temperature") && i + 1 < argc) tui_T = std::stod(argv[++i]);
            else if ((a == "-P" || a == "--pressure")    && i + 1 < argc) tui_P = std::stod(argv[++i]);
            else if ((a == "-n" || a == "--moles")       && i + 1 < argc) tui_n = std::stod(argv[++i]);
        }
        return gas2_tui_run(tui_formula, tui_T, tui_P, tui_n);
    }

    // Parse common options
    std::string formula;
    double T = 298.15, P = 1.0, n = 1.0;
    size_t count = 10000;
    uint64_t seed = 42;
    int grid_size = 64;
    bool json_out = false, compact = false;

    if (argc >= 4 && argv[3][0] != '-') {
        formula = argv[3];
    }

    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-T" || arg == "--temperature") && i + 1 < argc) T = std::stod(argv[++i]);
        else if ((arg == "-P" || arg == "--pressure") && i + 1 < argc) P = std::stod(argv[++i]);
        else if ((arg == "-n" || arg == "--moles") && i + 1 < argc) n = std::stod(argv[++i]);
        else if ((arg == "-N" || arg == "--count") && i + 1 < argc) count = std::stoull(argv[++i]);
        else if (arg == "--grid" && i + 1 < argc) grid_size = std::stoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
        else if (arg == "--json") json_out = true;
        else if (arg == "--compact") compact = true;
    }

    // ---- analyze ----
    if (subcmd == "analyze") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 analyze' requires a formula.\n";
            return 1;
        }
        auto a = analyze(formula, T, P, n, seed);
        if (json_out) std::cout << a.to_json() << "\n";
        else if (compact) std::cout << a.format_compact() << "\n";
        else std::cout << a.format_full_report();
        return 0;
    }

    // ---- thermal ----
    if (subcmd == "thermal") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 thermal' requires a formula.\n";
            return 1;
        }
        auto r = thermal_report(formula, T);
        std::cout << r.format();
        return 0;
    }

    // ---- compare ----
    if (subcmd == "compare") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 compare' requires a formula.\n";
            return 1;
        }
        auto a = analyze(formula, T, P, n, seed);
        std::cout << "\033[1;35mEOS Comparison: " << formula
                  << " at " << T << " K, " << P << " atm\033[0m\n\n";
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  " << std::setw(20) << std::left << "Equation"
                  << std::setw(14) << "Volume (L)"
                  << std::setw(12) << "Z"
                  << std::setw(10) << "Iters" << "\n";
        std::cout << "  " << std::string(56, '-') << "\n";
        std::cout << "  " << std::setw(20) << "Ideal Gas"
                  << std::setw(14) << a.eos_ideal.V_L()
                  << std::setw(12) << a.eos_ideal.Z
                  << std::setw(10) << a.eos_ideal.iterations << "\n";
        std::cout << "  " << std::setw(20) << "Van der Waals"
                  << std::setw(14) << a.eos_vdw.V_L()
                  << std::setw(12) << a.eos_vdw.Z
                  << std::setw(10) << a.eos_vdw.iterations << "\n";
        std::cout << "  " << std::setw(20) << "Redlich-Kwong"
                  << std::setw(14) << a.eos_rk.V_L()
                  << std::setw(12) << a.eos_rk.Z
                  << std::setw(10) << a.eos_rk.iterations << "\n";
        std::cout << "\n";
        double dev_vdw = (a.eos_vdw.V_L() - a.eos_ideal.V_L()) / a.eos_ideal.V_L() * 100.0;
        double dev_rk  = (a.eos_rk.V_L() - a.eos_ideal.V_L()) / a.eos_ideal.V_L() * 100.0;
        std::cout << std::setprecision(3);
        std::cout << "  VdW deviation from ideal: " << dev_vdw << " %\n";
        std::cout << "  R-K deviation from ideal: " << dev_rk << " %\n";
        return 0;
    }

    // ---- sample ----
    if (subcmd == "sample") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 sample' requires a formula.\n";
            return 1;
        }
        auto* sp = find_species(formula);
        double M_kg = sp ? sp->molar_mass_kg() : 0.028;
        auto samples = sample_mb(T, M_kg, count, seed);

        double sum_sq = 0.0, sum_sp = 0.0;
        double min_speed = 1e30, max_speed_val = 0.0;
        for (const auto& v : samples) {
            double s = v.speed();
            sum_sp += s;
            sum_sq += s * s;
            if (s < min_speed) min_speed = s;
            if (s > max_speed_val) max_speed_val = s;
        }
        double rms_s = std::sqrt(sum_sq / samples.size());
        double mean_s = sum_sp / samples.size();
        double rms_th = rms_speed(T, M_kg);
        double mean_th = mean_speed(T, M_kg);
        double mp_th = most_probable_speed(T, M_kg);
        double m_mol = M_kg / N_A;

        // ---- Header ----
        std::cout << "\n\033[1;38;5;213m"
                  << "╔══════════════════════════════════════════════════════════════════════╗\n"
                  << "║  ⚛  gas2 Maxwell-Boltzmann Sampling                                ║\n"
                  << "║     " << std::setw(65) << std::left << formula << "║\n"
                  << "╚══════════════════════════════════════════════════════════════════════╝\n"
                  << "\033[0m\n";

        // ---- Conditions ----
        std::cout << "\033[38;5;75m┌─ Conditions\033[0m\n";
        std::cout << "\033[38;5;250m│\033[0m  Temperature:  \033[1;38;5;214m" << std::fixed << std::setprecision(2) << T << " K\033[0m\n";
        std::cout << "\033[38;5;250m│\033[0m  Molar mass:   \033[38;5;250m" << (M_kg * 1000.0) << " g/mol\033[0m\n";
        std::cout << "\033[38;5;250m│\033[0m  Samples:      \033[1;38;5;123m" << count << "\033[0m\n";
        std::cout << "\033[38;5;250m│\033[0m  Seed:         \033[38;5;245m" << seed << "\033[0m\n";
        if (sp) {
            std::cout << "\033[38;5;250m│\033[0m  Species:      \033[1;38;5;219m" << sp->name << "\033[0m\n";
        }
        std::cout << "\033[38;5;250m│\033[0m\n";

        // ---- Speed Statistics ----
        std::cout << "\033[38;5;75m┌─ Speed Statistics\033[0m\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "\033[38;5;250m│\033[0m                Sampled    Theoretical     Δ\n";
        std::cout << "\033[38;5;250m│\033[0m  v_rms  = \033[1;38;5;156m" << std::setw(10) << rms_s
                  << "\033[0;38;5;245m  " << std::setw(10) << rms_th
                  << "\033[0m  " << std::setprecision(2) << std::showpos
                  << ((rms_s - rms_th) / rms_th * 100.0) << " %"
                  << std::noshowpos << "\n";
        std::cout << "\033[38;5;250m│\033[0m  v_mean = \033[1;38;5;156m" << std::setprecision(1)
                  << std::setw(10) << mean_s
                  << "\033[0;38;5;245m  " << std::setw(10) << mean_th
                  << "\033[0m  " << std::setprecision(2) << std::showpos
                  << ((mean_s - mean_th) / mean_th * 100.0) << " %"
                  << std::noshowpos << "\n";
        std::cout << "\033[38;5;250m│\033[0m  v_mp   = \033[38;5;245m" << std::setprecision(1)
                  << std::setw(10) << "—"
                  << "\033[0;38;5;245m  " << std::setw(10) << mp_th << "\033[0m  m/s\n";
        std::cout << "\033[38;5;250m│\033[0m\n";

        // ---- Speed Distribution Histogram (terminal bar chart) ----
        const int n_bins = 40;
        const int bar_height = 12;
        std::vector<int> hist(n_bins, 0);
        double bin_width = (max_speed_val * 1.05) / n_bins;
        for (const auto& v : samples) {
            int b = static_cast<int>(v.speed() / bin_width);
            if (b >= 0 && b < n_bins) hist[b]++;
        }
        int hist_max = *std::max_element(hist.begin(), hist.end());
        if (hist_max == 0) hist_max = 1;

        std::cout << "\033[38;5;75m┌─ Speed Distribution  f(v)                                    ┐\033[0m\n";
        // Column render (top to bottom)
        for (int row = bar_height; row >= 1; --row) {
            double threshold = static_cast<double>(row) / bar_height * hist_max;
            std::cout << "\033[38;5;250m│\033[0m ";
            if (row == bar_height)
                std::cout << std::setw(5) << hist_max << " ";
            else if (row == bar_height / 2)
                std::cout << std::setw(5) << (hist_max / 2) << " ";
            else if (row == 1)
                std::cout << "    0 ";
            else
                std::cout << "      ";
            for (int b = 0; b < n_bins; ++b) {
                if (hist[b] >= threshold) {
                    // Colour: gradient from cool (blue) to hot (red) based on speed bin
                    int ci = 17 + static_cast<int>(static_cast<double>(b) / n_bins * 214);
                    if (ci > 231) ci = 231;
                    std::cout << "\033[48;5;" << ci << "m \033[0m";
                } else {
                    std::cout << " ";
                }
            }
            // Markers for v_mp, v_mean, v_rms
            if (row == bar_height) {
                std::cout << "  \033[38;5;226m◆\033[0m v_mp";
            } else if (row == bar_height - 1) {
                std::cout << "  \033[38;5;48m◆\033[0m v_mean";
            } else if (row == bar_height - 2) {
                std::cout << "  \033[38;5;196m◆\033[0m v_rms";
            }
            std::cout << "\n";
        }
        // X-axis
        std::cout << "\033[38;5;250m│\033[0m       ";
        for (int b = 0; b < n_bins; ++b) std::cout << "\033[38;5;240m─\033[0m";
        std::cout << "\n";
        std::cout << "\033[38;5;250m│\033[0m       0";
        std::cout << std::string(n_bins / 2 - 4, ' ');
        std::cout << std::fixed << std::setprecision(0) << (max_speed_val * 0.5);
        std::cout << std::string(n_bins / 2 - 5, ' ');
        std::cout << std::setprecision(0) << max_speed_val << " m/s\n";

        // Marker positions on x-axis
        int mp_bin = static_cast<int>(mp_th / bin_width);
        int mn_bin = static_cast<int>(mean_s / bin_width);
        int rm_bin = static_cast<int>(rms_s / bin_width);
        std::cout << "\033[38;5;250m│\033[0m       ";
        for (int b = 0; b < n_bins; ++b) {
            if (b == mp_bin)      std::cout << "\033[38;5;226m▲\033[0m";
            else if (b == mn_bin) std::cout << "\033[38;5;48m▲\033[0m";
            else if (b == rm_bin) std::cout << "\033[38;5;196m▲\033[0m";
            else                  std::cout << " ";
        }
        std::cout << "\n\033[38;5;250m│\033[0m\n";

        // ---- Energy Sparkline (KE per sample, mini line) ----
        const int spark_w = 60;
        std::cout << "\033[38;5;75m┌─ KE Sparkline (first " << spark_w << " samples)\033[0m\n";
        std::cout << "\033[38;5;250m│\033[0m  ";
        const char* spark_chars[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
        double ke_spark_max = 0.0;
        for (int i = 0; i < spark_w && i < static_cast<int>(samples.size()); ++i) {
            double ke = samples[i].ke(m_mol);
            if (ke > ke_spark_max) ke_spark_max = ke;
        }
        if (ke_spark_max < 1e-30) ke_spark_max = 1e-20;
        for (int i = 0; i < spark_w && i < static_cast<int>(samples.size()); ++i) {
            double ke = samples[i].ke(m_mol);
            int level = static_cast<int>(ke / ke_spark_max * 7.0);
            if (level < 0) level = 0;
            if (level > 7) level = 7;
            // Colour by energy level
            int ci = 22 + level * 6;
            std::cout << "\033[38;5;" << ci << "m" << spark_chars[level] << "\033[0m";
        }
        std::cout << "\n\033[38;5;250m│\033[0m\n";

        // ---- vx-vy Mini Scatter (small terminal grid) ----
        const int sc_size = 20;
        std::vector<int> sc_grid(sc_size * sc_size, 0);
        double sc_max_v = max_speed_val * 1.05;
        for (const auto& v : samples) {
            int bx = static_cast<int>((v.vx + sc_max_v) / (2.0 * sc_max_v) * sc_size);
            int by = static_cast<int>((v.vy + sc_max_v) / (2.0 * sc_max_v) * sc_size);
            if (bx >= 0 && bx < sc_size && by >= 0 && by < sc_size)
                sc_grid[by * sc_size + bx]++;
        }
        int sc_max = *std::max_element(sc_grid.begin(), sc_grid.end());
        if (sc_max == 0) sc_max = 1;
        const char* density_chars[] = {" ", "·", "∘", "○", "●", "◉", "◎", "⬤"};

        std::cout << "\033[38;5;75m┌─ vx–vy  Velocity Scatter\033[0m\n";
        for (int row = 0; row < sc_size; ++row) {
            std::cout << "\033[38;5;250m│\033[0m  ";
            for (int col = 0; col < sc_size; ++col) {
                int val = sc_grid[row * sc_size + col];
                int level = static_cast<int>(static_cast<double>(val) / sc_max * 7.0);
                if (level > 7) level = 7;
                // Colour ramp: dark blue to white
                int ci;
                if (level == 0) ci = 0;
                else ci = 17 + level * 30;
                if (ci > 231) ci = 231;
                if (level == 0)
                    std::cout << " ";
                else
                    std::cout << "\033[38;5;" << ci << "m" << density_chars[level] << "\033[0m";
            }
            if (row == 0)          std::cout << "  +vy";
            else if (row == sc_size / 2) std::cout << "   0";
            else if (row == sc_size - 1) std::cout << "  -vy";
            std::cout << "\n";
        }
        std::cout << "\033[38;5;250m│\033[0m  ";
        for (int c = 0; c < sc_size; ++c) std::cout << "\033[38;5;240m─\033[0m";
        std::cout << "\n";
        std::cout << "\033[38;5;250m│\033[0m  -vx" << std::string(sc_size - 8, ' ') << "+vx\n";
        std::cout << "\033[38;5;250m│\033[0m\n";

        // ---- Accuracy verdict ----
        double err_pct = std::abs(rms_s - rms_th) / rms_th * 100.0;
        std::cout << "\033[38;5;75m┌─ Verdict\033[0m\n";
        if (err_pct < 1.0) {
            std::cout << "\033[38;5;250m│\033[0m  \033[38;5;48m✓ EXCELLENT\033[0m  RMS error " << std::setprecision(3) << err_pct << "%  ─  distribution well-sampled\n";
        } else if (err_pct < 3.0) {
            std::cout << "\033[38;5;250m│\033[0m  \033[38;5;226m✓ GOOD\033[0m       RMS error " << std::setprecision(3) << err_pct << "%  ─  within statistical noise\n";
        } else if (err_pct < 5.0) {
            std::cout << "\033[38;5;250m│\033[0m  \033[38;5;214m○ FAIR\033[0m       RMS error " << std::setprecision(3) << err_pct << "%  ─  increase -N for better convergence\n";
        } else {
            std::cout << "\033[38;5;250m│\033[0m  \033[38;5;196m✗ POOR\033[0m       RMS error " << std::setprecision(3) << err_pct << "%  ─  needs more samples\n";
        }
        std::cout << "\033[38;5;250m└\033[0m\n\n";
        return 0;
    }

    // ---- phase ----
    if (subcmd == "phase") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 phase' requires a formula.\n";
            return 1;
        }
        auto* sp = find_species(formula);
        if (!sp) {
            std::cerr << "Unknown species: " << formula << "\n";
            return 1;
        }

        std::cout << "\033[1;35m"
                  << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n"
                  << "\u2551  gas2 Phase Equilibrium: " << std::setw(37) << std::left << formula << "  \u2551\n"
                  << "\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n"
                  << "\033[0m\n";

        // Helmholtz
        auto eos_v = vdw_solve_volume(n, T, P * atm_to_Pa, sp->vdw_a, sp->vdw_b);
        auto helm = helmholtz(n, T, eos_v.V_m3, sp->molar_mass_g,
                              sp->vdw_a, sp->vdw_b, EOSType::VanDerWaals);
        std::cout << "\033[1;36m\u250c\u2500 Helmholtz Free Energy A(T,V,N)\033[0m\n";
        std::cout << std::scientific << std::setprecision(6);
        std::cout << "\u2502  A_ideal = " << helm.A_ig << " J\n";
        std::cout << "\u2502  A_dep   = " << helm.A_dep << " J (VdW departure)\n";
        std::cout << "\u2502  A_total = " << helm.A_total << " J\n";
        std::cout << "\u2502  A_total = " << helm.A_total_Eh << " Eh\n";
        std::cout << "\u2502  \u039b       = " << helm.Lambda_m << " m (thermal wavelength)\n";
        std::cout << "\u2502\n";

        // Gibbs
        auto gib = gibbs(n, T, P * atm_to_Pa, sp, EOSType::VanDerWaals);
        std::cout << "\033[1;36m\u250c\u2500 Gibbs Free Energy G(T,P,N)\033[0m\n";
        std::cout << "\u2502  G       = " << gib.G << " J\n";
        std::cout << "\u2502  G       = " << gib.G_Eh << " Eh\n";
        std::cout << "\u2502  A       = " << gib.A << " J\n";
        std::cout << "\u2502  PV      = " << gib.PV << " J\n";
        std::cout << "\u2502  Z       = " << std::fixed << std::setprecision(6) << gib.Z << "\n";
        std::cout << "\u2502\n";

        // Chemical potential
        auto mu = chemical_potential(T, P * atm_to_Pa, sp, EOSType::VanDerWaals);
        std::cout << "\033[1;36m\u250c\u2500 Chemical Potential \u03bc(T,P)\033[0m\n";
        std::cout << std::scientific << std::setprecision(6);
        std::cout << "\u2502  \u03bc      = " << mu.mu_J << " J/molecule\n";
        std::cout << "\u2502  \u03bc      = " << mu.mu_Eh << " Eh\n";
        std::cout << "\u2502  \u03bc      = " << std::fixed << std::setprecision(6) << mu.mu_eV << " eV\n";
        std::cout << "\u2502  \u03bc      = " << std::setprecision(3) << mu.mu_kJmol << " kJ/mol\n";
        std::cout << "\u2502  \u03c6      = " << std::setprecision(6) << mu.phi << " (fugacity coeff)\n";
        std::cout << "\u2502  f      = " << std::scientific << mu.f_Pa << " Pa (fugacity)\n";
        std::cout << "\u2502\n";

        // Entropy
        auto entr = entropy_ideal(n, T, eos_v.V_m3, sp->molar_mass_g);
        std::cout << "\033[1;36m\u250c\u2500 Entropy S(T,V,N) \u2014 Sackur-Tetrode\033[0m\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "\u2502  S       = " << entr.S_per_mol << " J/(mol\u00b7K)\n";
        std::cout << "\u2502\n";

        // Maxwell construction
        double Tc = 8.0 * sp->vdw_a / (27.0 * R_gas * sp->vdw_b);
        if (T < Tc) {
            auto pt = maxwell_construction(T, sp->vdw_a, sp->vdw_b);
            std::cout << "\033[1;36m\u250c\u2500 Phase Boundary (Maxwell Construction)\033[0m\n";
            if (pt.converged) {
                std::cout << std::scientific << std::setprecision(4);
                std::cout << "\u2502  P_sat   = " << pt.P_sat_Pa << " Pa  ("
                          << std::fixed << std::setprecision(4)
                          << (pt.P_sat_Pa / atm_to_Pa) << " atm)\n";
                std::cout << std::scientific << std::setprecision(4);
                std::cout << "\u2502  V_liq   = " << pt.V_liq_m3 << " m\u00b3/mol\n";
                std::cout << "\u2502  V_vap   = " << pt.V_vap_m3 << " m\u00b3/mol\n";
                std::cout << std::fixed << std::setprecision(6);
                std::cout << "\u2502  Z_liq   = " << pt.Z_liq << "\n";
                std::cout << "\u2502  Z_vap   = " << pt.Z_vap << "\n";
                std::cout << "\u2502  iters   = " << pt.iterations << "\n";
            } else {
                std::cout << "\u2502  (did not converge)\n";
            }
        } else {
            std::cout << "\033[1;36m\u250c\u2500 Phase Boundary\033[0m\n";
            std::cout << "\u2502  T = " << std::fixed << std::setprecision(1) << T
                      << " K >= Tc = " << Tc << " K  (supercritical, no phase boundary)\n";
        }
        std::cout << "\u2514\n";
        return 0;
    }

    // ---- potentials ----
    if (subcmd == "potentials") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 potentials' requires a formula.\n";
            return 1;
        }
        auto* sp = find_species(formula);

        std::cout << "\033[1;35m"
                  << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n"
                  << "\u2551  gas2 Potential Decomposition + F[\u03c6]                         \u2551\n"
                  << "\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n"
                  << "\033[0m\n";

        // Build a representative potential decomposition from species data
        PotentialDecomposition U{};
        if (sp) {
            // For a gas species at given T, P: dominant terms are vdW + kinetic
            double Vm = R_gas * T / (P * atm_to_Pa);  // Ideal molar volume
            double r_avg = std::cbrt(Vm / N_A);  // Average intermolecular distance
            double sigma = sp->d_kinetic_m();
            double eps = sp->vdw_a / (27.0 * sp->vdw_b * sp->vdw_b * N_A);

            // LJ estimate at average separation
            double sr6 = std::pow(sigma / r_avg, 6);
            U.U_vdw = 4.0 * eps * N_A * (sr6 * sr6 - sr6);  // J/mol
            U.n_pairs = static_cast<int>(N_A);  // order of magnitude

            // Mono/diatomic: no bonds/angles/torsions for noble gases
            if (sp->n_atoms > 1) {
                // Placeholder: representative bond energy from formation enthalpy
                U.U_bond = sp->Hf0_kJmol * 1000.0;  // J/mol (rough)
                U.n_bonds = sp->n_atoms - 1;
            }

            // Electrostatic estimate (zero for noble, nonzero for polar)
            // Placeholder: use Cp-Cv difference as proxy for intermolecular strength
            U.U_coul = 0.0;  // Pure gas at low P
            U.U_pol = 0.0;
            U.U_many = 0.0;
        }
        U.recompute_total();

        std::cout << "\033[1;36m\u250c\u2500 8-Channel Potential Decomposition\033[0m\n";
        std::cout << U.format_table();
        std::cout << "\u2502\n";

        // Landau-Ginzburg F[phi]
        double Tc = sp ? sp->Tc_K : 300.0;
        double sigma = sp ? sp->d_kinetic_m() : 3.4e-10;
        double eps = sp ? sp->vdw_a / (27.0 * sp->vdw_b * sp->vdw_b * N_A) : 1e-21;
        auto lp = LandauParams::from_species(T, Tc, sigma, eps);

        // Build 1D density profile with tanh interface
        DensityProfile prof;
        double phi_eq = std::sqrt(std::abs(lp.a) / (2.0 * lp.b + 1e-30));
        prof.init_tanh_interface(100, 100.0 * sigma, phi_eq, 5.0 * sigma);
        auto F = evaluate_free_energy(prof, lp);

        std::cout << "\033[1;36m\u250c\u2500 Landau-Ginzburg Free Energy F[\u03c6]\033[0m\n";
        std::cout << std::scientific << std::setprecision(4);
        std::cout << "\u2502  a      = " << lp.a << " (sign: "
                  << (lp.a > 0 ? "T > Tc, disordered" : "T < Tc, ordered") << ")\n";
        std::cout << "\u2502  b      = " << lp.b << "\n";
        std::cout << "\u2502  \u03ba      = " << lp.kappa << " (gradient penalty)\n";
        std::cout << "\u2502  \u03c6_eq   = " << phi_eq << " (equilibrium order param)\n";
        std::cout << "\u2502  F_bulk = " << F.F_bulk << " J\n";
        std::cout << "\u2502  F_grad = " << F.F_gradient << " J\n";
        std::cout << "\u2502  F_total= " << F.F_total << " J\n";
        std::cout << "\u2502  F_total= " << F.F_total_Eh << " Eh\n";
        std::cout << "\u2514\n";
        return 0;
    }

    // ---- monitor ----
    if (subcmd == "monitor") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 monitor' requires a formula.\n";
            return 1;
        }
        auto* sp = find_species(formula);
        if (!sp) {
            std::cerr << "Unknown species: " << formula << "\n";
            return 1;
        }

        // Write JSON snapshots to stdout, one per line, for the Tkinter monitor
        double Tc = sp->Tc_K;
        double sigma = sp->d_kinetic_m();
        double eps = sp->vdw_a / (27.0 * sp->vdw_b * sp->vdw_b * N_A);

        int n_cycles = static_cast<int>(count);  // reuse --count
        if (n_cycles <= 0) n_cycles = 100;

        for (int cyc = 0; cyc < n_cycles; ++cyc) {
            double T_cyc = T + 0.5 * cyc;  // Slowly sweep temperature

            MonitorSnapshot snap{};
            snap.cycle = cyc;
            snap.T_K = T_cyc;
            snap.formula = formula;

            // Potentials
            double Vm = R_gas * T_cyc / (P * atm_to_Pa);
            double r_avg = std::cbrt(Vm / N_A);
            double sr6 = std::pow(sigma / r_avg, 6);
            snap.U.U_vdw = 4.0 * eps * N_A * (sr6 * sr6 - sr6);
            if (sp->n_atoms > 1) {
                snap.U.U_bond = sp->Hf0_kJmol * 1000.0;
                snap.U.n_bonds = sp->n_atoms - 1;
            }
            snap.U.recompute_total();

            // F[phi]
            auto lp = LandauParams::from_species(T_cyc, Tc, sigma, eps);
            snap.landau = lp;
            double phi_eq = std::sqrt(std::abs(lp.a) / (2.0 * lp.b + 1e-30));
            snap.profile.init_tanh_interface(100, 100.0 * sigma, phi_eq, 5.0 * sigma);
            snap.F = evaluate_free_energy(snap.profile, lp);

            std::cout << snap.to_json() << "\n";
            std::cout.flush();
        }
        return 0;
    }

    // ---- heatmap ----
    if (subcmd == "heatmap") {
        if (formula.empty()) {
            std::cerr << "ERROR: 'gas2 heatmap' requires a formula.\n";
            return 1;
        }
        auto* sp = find_species(formula);
        double M_kg = sp ? sp->molar_mass_kg() : 0.028;

        // Clamp grid to supported sizes
        if (grid_size != 48 && grid_size != 64 && grid_size != 256)
            grid_size = 64;

        // Heat map resolution (always square)
        const int hm_size = grid_size;  // vv and se map resolution
        const int shell_size = grid_size;

        // Continuous frame loop — 48× slowdown via temperature crawl
        // Each frame: re-sample MB, bin, stream JSON
        const int n_frames = static_cast<int>(count);  // reuse -N for frame count
        const double T_step = 0.02;  // K per frame (slow crawl)

        for (int frame = 0; frame < n_frames; ++frame) {
            double T_cur = T + T_step * frame;
            uint64_t frame_seed = seed + static_cast<uint64_t>(frame);

            // Sample 10000 velocities
            const size_t n_samp = 10000;
            auto samples = sample_mb(T_cur, M_kg, n_samp, frame_seed);

            // Compute per-sample speed and KE
            double m_molecule = M_kg / N_A;
            std::vector<double> speeds(n_samp), kes(n_samp);
            double max_speed = 0.0, max_ke = 0.0;
            double sum_ke = 0.0;
            for (size_t i = 0; i < n_samp; ++i) {
                speeds[i] = samples[i].speed();
                kes[i] = samples[i].ke(m_molecule);
                if (speeds[i] > max_speed) max_speed = speeds[i];
                if (kes[i] > max_ke) max_ke = kes[i];
                sum_ke += kes[i];
            }
            double vmax = max_speed * 1.1;  // Padding for binning
            double ke_max_bin = max_ke * 1.1;
            if (vmax < 1e-10) vmax = 1.0;
            if (ke_max_bin < 1e-30) ke_max_bin = 1e-20;

            // ---- vx-vy heat map (2D histogram) ----
            std::vector<double> vv_map(hm_size * hm_size, 0.0);
            for (size_t i = 0; i < n_samp; ++i) {
                int bx = static_cast<int>((samples[i].vx + vmax) / (2.0 * vmax) * hm_size);
                int by = static_cast<int>((samples[i].vy + vmax) / (2.0 * vmax) * hm_size);
                if (bx >= 0 && bx < hm_size && by >= 0 && by < hm_size)
                    vv_map[by * hm_size + bx] += 1.0;
            }

            // ---- speed-KE heat map ----
            std::vector<double> se_map(hm_size * hm_size, 0.0);
            for (size_t i = 0; i < n_samp; ++i) {
                int bx = static_cast<int>(speeds[i] / vmax * hm_size);
                int by = static_cast<int>(kes[i] / ke_max_bin * hm_size);
                if (bx >= 0 && bx < hm_size && by >= 0 && by < hm_size)
                    se_map[(hm_size - 1 - by) * hm_size + bx] += 1.0;
            }

            // ---- atom grid (NxN lattice, assign atoms round-robin) ----
            int n_grid_cells = grid_size * grid_size;
            std::vector<double> atom_grid(n_grid_cells, 0.0);
            // Place samples onto grid cells by index modulo, accumulate KE
            std::vector<int> cell_count(n_grid_cells, 0);
            for (size_t i = 0; i < n_samp; ++i) {
                int cell = static_cast<int>(i % n_grid_cells);
                atom_grid[cell] += kes[i];
                cell_count[cell]++;
            }
            // Average KE per cell
            for (int j = 0; j < n_grid_cells; ++j) {
                if (cell_count[j] > 0)
                    atom_grid[j] /= cell_count[j];
            }

            // ---- shell matrix (radial speed bins, NxN) ----
            // Map (vx,vy) into radial shell (distance from origin) vs angle
            std::vector<double> shell_map(shell_size * shell_size, 0.0);
            for (size_t i = 0; i < n_samp; ++i) {
                double r = std::sqrt(samples[i].vx * samples[i].vx +
                                     samples[i].vy * samples[i].vy);
                double angle = std::atan2(samples[i].vy, samples[i].vx);
                // angle in [-pi, pi] -> [0, 1]
                double a_norm = (angle + PI) / (2.0 * PI);
                int br = static_cast<int>(r / vmax * shell_size);
                int ba = static_cast<int>(a_norm * shell_size);
                if (br >= 0 && br < shell_size && ba >= 0 && ba < shell_size)
                    shell_map[br * shell_size + ba] += 1.0;
            }

            // ---- Write JSON frame ----
            std::ostringstream js;
            js << std::scientific << std::setprecision(6);
            js << "{";
            js << "\"cycle\":" << frame << ",";
            js << "\"T_K\":" << T_cur << ",";
            js << "\"formula\":\"" << formula << "\",";
            js << "\"n_samples\":" << n_samp << ",";
            js << "\"v_rms\":" << rms_speed(T_cur, M_kg) << ",";
            js << "\"ke_avg_Eh\":" << (sum_ke / n_samp) / Hartree_J << ",";
            js << "\"grid_size\":" << grid_size << ",";

            // vv heat map
            js << "\"vv_size\":" << hm_size << ",";
            js << "\"vv_heatmap\":[";
            for (int j = 0; j < hm_size * hm_size; ++j) {
                if (j > 0) js << ",";
                js << vv_map[j];
            }
            js << "],";

            // se heat map
            js << "\"se_size\":" << hm_size << ",";
            js << "\"se_heatmap\":[";
            for (int j = 0; j < hm_size * hm_size; ++j) {
                if (j > 0) js << ",";
                js << se_map[j];
            }
            js << "],";

            // atom grid
            js << "\"atom_grid\":[";
            for (int j = 0; j < n_grid_cells; ++j) {
                if (j > 0) js << ",";
                js << atom_grid[j];
            }
            js << "],";

            // shell matrix
            js << "\"shell_size\":" << shell_size << ",";
            js << "\"shell_matrix\":[";
            for (int j = 0; j < shell_size * shell_size; ++j) {
                if (j > 0) js << ",";
                js << shell_map[j];
            }
            js << "]";

            js << "}";
            std::cout << js.str() << "\n";
            std::cout.flush();
        }
        return 0;
    }

    // ---- species ----
    if (subcmd == "species") {
        if (!formula.empty()) {
            auto* sp = find_species(formula);
            if (!sp) {
                std::cerr << "Species not found: " << formula << "\n";
                return 1;
            }
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "\033[1m" << sp->name << " (" << sp->formula << ")\033[0m\n";
            std::cout << "  Molar mass:    " << sp->molar_mass_g << " g/mol\n";
            std::cout << "  Atoms/mol:     " << sp->n_atoms << "\n";
            std::cout << "  Cp:            " << sp->Cp_Jmol << " J/(mol·K)\n";
            std::cout << "  Cv:            " << sp->Cv_Jmol << " J/(mol·K)\n";
            std::cout << "  γ:             " << sp->gamma << "\n";
            std::cout << "  ΔHf°:          " << sp->Hf0_kJmol << " kJ/mol\n";
            std::cout << "  Viscosity:     " << sp->viscosity_uPas << " μPa·s\n";
            std::cout << "  k_thermal:     " << sp->k_thermal_mWmK << " mW/(m·K)\n";
            std::cout << "  d_kinetic:     " << sp->d_kinetic_pm << " pm\n";
            std::cout << "  VdW a:         " << sp->vdw_a << " Pa·m⁶/mol²\n";
            std::cout << "  VdW b:         " << std::setprecision(6) << sp->vdw_b << " m³/mol\n";
            std::cout << "  Tc:            " << std::setprecision(2) << sp->Tc_K << " K\n";
            std::cout << "  Pc:            " << sp->Pc_atm << " atm\n";
            std::cout << "  Vc:            " << sp->Vc_cm3mol << " cm³/mol\n";
            return 0;
        }
        // List all species
        std::cout << "\033[1;35mgas2 Species Database\033[0m\n\n";
        std::cout << std::setw(8) << std::left << "Formula"
                  << std::setw(22) << "Name"
                  << std::setw(10) << "M (g/mol)"
                  << std::setw(8) << "γ"
                  << std::setw(10) << "Tc (K)"
                  << std::setw(10) << "Pc (atm)" << "\n";
        std::cout << std::string(68, '-') << "\n";
        for (const auto& [key, sp] : species_database()) {
            std::cout << std::fixed << std::setprecision(3);
            std::cout << std::setw(8) << std::left << sp.formula
                      << std::setw(22) << sp.name
                      << std::setw(10) << sp.molar_mass_g
                      << std::setw(8) << sp.gamma
                      << std::setw(10) << std::setprecision(1) << sp.Tc_K
                      << std::setw(10) << sp.Pc_atm << "\n";
        }
        std::cout << "\n" << species_database().size() << " species registered.\n";
        return 0;
    }

    std::cerr << "Unknown gas2 subcommand: " << subcmd << "\n";
    std::cerr << "Run 'vsepr gas2 help' for usage.\n";
    return 1;
}

} // namespace gas2
} // namespace vsepr
