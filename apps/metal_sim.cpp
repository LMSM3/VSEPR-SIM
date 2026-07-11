/**
 * metal_sim.cpp — Interactive Metals Research Simulator
 *
 * An interactive, user-centric terminal application for metals research.
 * Uses the 6+9 FIRE SeedBead engine with metal-class-tuned parameters.
 *
 * Modes:
 *   1. Interactive REPL   — pick metals, tune options, run, inspect results
 *   2. Batch sweep        — run all metals in registry, output full report
 *   3. Alloy pair study   — select A+B, composition x_B, run and compare
 *   4. Quick single       — metal_sim Au 64   (non-interactive)
 *
 * Usage:
 *   metal_sim                       → interactive REPL
 *   metal_sim --batch               → all metals batch sweep
 *   metal_sim --alloys              → all canonical alloy pairs
 *   metal_sim <SYMBOL> [n_beads]    → quick single run
 *   metal_sim <A> <B> [x_B] [n]    → quick alloy run
 *
 * All outputs:
 *   metals_report.md      — Markdown research report (always written)
 *   metals_results.csv    — per-metal row table
 *   alloy_pairs.csv       — alloy pair descriptor table (if alloy mode)
 *
 * Anti-black-box: every FIRE parameter, bead count, convergence step,
 * and comparison metric is printed and written to file.
 */

#include "coarse_grain/metals/metal_registry.hpp"
#include "coarse_grain/metals/metal_fire_params.hpp"
#include "coarse_grain/metals/metal_reporter.hpp"
#include "coarse_grain/metals/metal_sim_builder.hpp"
#include "coarse_grain/metals/radiation_interaction.hpp"
#include "coarse_grain/metals/scattering_pattern.hpp"
#include "coarse_grain/metals/energetic_signature.hpp"
#include "coarse_grain/theory/energy_decomposition.hpp"
#include "coarse_grain/theory/graph_topology.hpp"
#include "coarse_grain/theory/material_kernel.hpp"
#include "coarse_grain/database/demo_entries.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/seed_bead_stepper.hpp"
#include "coarse_grain/level3/level3_builder.hpp"
#include "coarse_grain/analysis/macro_precursor.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace coarse_grain;
using namespace coarse_grain::metals;

// ============================================================================
// Run a single metal FIRE simulation
// ============================================================================

MetalSimResult run_metal(const MetalRecord& metal,
                         uint32_t n_beads,
                         bool print_live,
                         bool enable_l3 = true)
{
    MetalSimResult result;
    result.metal   = metal;
    result.n_beads = n_beads;

    // Build system + parameters
    BeadSystem sys = build_metal_bead_system(metal, n_beads);
    MetalFireParams mfp = params_for_metal(metal);
    SeedBeadParams& params = mfp.seed;

    const size_t N = sys.beads.size();
    result.n_beads = static_cast<uint32_t>(N);

    std::vector<EnvironmentState> env_states(N);
    std::vector<atomistic::Vec3>  velocities(N, {0,0,0});
    std::vector<atomistic::Vec3>  forces(N,    {0,0,0});
    FIREState fire;
    fire.dt    = params.dt_initial;
    fire.alpha = params.fire_alpha_start;

    SeedBeadStepper::init(sys, env_states, params);

    if (print_live) {
        std::cout << "\n  Running " << metal.symbol << " [" << crystal_structure_name(metal.structure) << "]"
                  << "  N=" << N << "  τ=" << params.env_params.tau
                  << "  γ_s=" << params.env_params.gamma_steric
                  << "  γ_d=" << params.env_params.gamma_disp << "\n";
        std::cout << "  " << std::string(62, '-') << "\n";
        std::cout << "  " << std::left
                  << std::setw(8)  << "Step"
                  << std::setw(14) << "RMS Force"
                  << std::setw(12) << "Energy"
                  << std::setw(10) << "η̄"
                  << std::setw(10) << "C̄"
                  << "Conv\n";
        std::cout << "  " << std::string(62, '-') << "\n";
    }

    auto t0 = std::chrono::high_resolution_clock::now();

    uint64_t print_interval = std::max(params.max_steps / 20, uint64_t(1));

    for (uint64_t s = 0; s < params.max_steps; ++s) {
        auto rec = SeedBeadStepper::step(
            sys, env_states, velocities, forces, fire, params, s);

        if (print_live && (s % print_interval == 0 || rec.steady_state)) {
            std::cout << "  "
                      << std::setw(8)  << s
                      << std::setw(14) << std::fixed << std::setprecision(5) << rec.rms_force
                      << std::setw(12) << std::setprecision(4) << rec.total_energy
                      << std::setw(10) << std::setprecision(4) << rec.avg_eta
                      << std::setw(10) << std::setprecision(2) << rec.avg_C;
            if (rec.steady_state) std::cout << "  ✓ CONVERGED";
            std::cout << "\n";
        }

        if (rec.steady_state) {
            result.converged      = true;
            result.steps_taken    = s + 1;
            result.final_rms_force = rec.rms_force;
            result.final_avg_eta  = rec.avg_eta;
            result.final_avg_rho  = rec.avg_rho;
            result.final_avg_C    = rec.avg_C;
            result.final_energy   = rec.total_energy;
            break;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (!result.converged) {
        result.steps_taken = params.max_steps;
    }

    // L3 domain aggregation
    if (enable_l3 && N >= 8) {
        std::vector<bool> conv_flags(N, result.converged);
        std::string hash_str = metal.symbol + "_" + std::to_string(n_beads);
        auto l3_records = coarse_grain::level3::aggregate_to_l3(
            sys.beads, env_states, conv_flags, hash_str, {});

        result.n_l3_domains = static_cast<int>(l3_records.size());
        if (!l3_records.empty()) {
            auto& ms = l3_records[0].macro_state;
            if (ms.rigidity_like.valid)    result.macro_rigidity  = ms.rigidity_like.value;
            if (ms.ductility_like.valid)   result.macro_ductility = ms.ductility_like.value;
            if (ms.cohesion_integrity_like.valid) result.macro_cohesion = ms.cohesion_integrity_like.value;
        }
    }

    return result;
}

// ============================================================================
// Run alloy pair simulation
// ============================================================================

MetalSimResult run_alloy(const MetalRecord& A,
                         const MetalRecord& B,
                         double x_B,
                         uint32_t n_beads,
                         bool print_live)
{
    MetalRecord alloy_meta;
    alloy_meta.symbol  = A.symbol + B.symbol;
    alloy_meta.name    = A.name + "-" + B.name + " alloy";
    alloy_meta.structure = (x_B < 0.5) ? A.structure : B.structure;
    alloy_meta.lj_sigma_ang    = (1 - x_B) * A.lj_sigma_ang   + x_B * B.lj_sigma_ang;
    alloy_meta.lj_epsilon_kcal = std::sqrt(A.lj_epsilon_kcal   * B.lj_epsilon_kcal);
    alloy_meta.atomic_mass_amu = (1 - x_B) * A.atomic_mass_amu + x_B * B.atomic_mass_amu;
    alloy_meta.bulk_CN         = (1 - x_B) * A.bulk_CN         + x_B * B.bulk_CN;
    alloy_meta.cohesive_energy_ev = (1 - x_B) * A.cohesive_energy_ev + x_B * B.cohesive_energy_ev;
    alloy_meta.lattice_constant_ang = (1 - x_B) * A.lattice_constant_ang + x_B * B.lattice_constant_ang;
    alloy_meta.source = "Alloy proxy: Vegard law mix";
    alloy_meta.is_noble_metal = A.is_noble_metal && B.is_noble_metal;
    alloy_meta.is_refractory  = A.is_refractory  || B.is_refractory;
    fill_lj_params(alloy_meta);

    MetalSimResult result;
    result.metal   = alloy_meta;
    result.n_beads = n_beads;

    BeadSystem sys = build_alloy_bead_system(A, B, n_beads, x_B);
    MetalFireParams mfp = alloy_params(A, B, x_B);
    SeedBeadParams& params = mfp.seed;

    const size_t N = sys.beads.size();
    result.n_beads = static_cast<uint32_t>(N);

    std::vector<EnvironmentState> env_states(N);
    std::vector<atomistic::Vec3>  velocities(N, {0,0,0});
    std::vector<atomistic::Vec3>  forces(N,    {0,0,0});
    FIREState fire;
    fire.dt    = params.dt_initial;
    fire.alpha = params.fire_alpha_start;

    SeedBeadStepper::init(sys, env_states, params);

    if (print_live) {
        std::cout << "\n  Running alloy " << A.symbol << std::setprecision(0)
                  << (1-x_B)*100 << "%-" << B.symbol << (int)(x_B*100) << "%"
                  << "  N=" << N << "\n";
    }

    uint64_t print_interval = std::max(params.max_steps / 10, uint64_t(1));

    auto t0 = std::chrono::high_resolution_clock::now();

    for (uint64_t s = 0; s < params.max_steps; ++s) {
        auto rec = SeedBeadStepper::step(
            sys, env_states, velocities, forces, fire, params, s);

        if (print_live && (s % print_interval == 0 || rec.steady_state)) {
            std::cout << "  step=" << std::setw(6) << s
                      << "  F_rms=" << std::fixed << std::setprecision(5) << rec.rms_force
                      << "  η̄=" << std::setprecision(4) << rec.avg_eta;
            if (rec.steady_state) std::cout << "  ✓";
            std::cout << "\n";
        }

        if (rec.steady_state) {
            result.converged      = true;
            result.steps_taken    = s + 1;
            result.final_rms_force = rec.rms_force;
            result.final_avg_eta  = rec.avg_eta;
            result.final_avg_rho  = rec.avg_rho;
            result.final_avg_C    = rec.avg_C;
            result.final_energy   = rec.total_energy;
            break;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    if (!result.converged) result.steps_taken = params.max_steps;

    return result;
}

// ============================================================================
// Output writers
// ============================================================================

void write_results_csv(const std::vector<MetalSimResult>& results,
                       const std::string& path)
{
    std::ofstream f(path);
    f << "symbol,name,structure,n_beads,converged,steps,rms_force,"
      << "avg_eta,avg_rho,avg_C,final_energy_kcal,elapsed_ms,"
      << "n_l3_domains,macro_rigidity,macro_ductility,macro_cohesion,"
      << "Ecoh_ev,Tmelt_K,a0_ang,B_GPa,G_GPa,E_GPa,"
      << "E_d_eV,mu_mass_100keV,k_edge_keV,dH_comb_kJg,T_ign_K,oxide,flame\n";
    for (const auto& r : results) {
        const auto& m = r.metal;
        f << m.symbol << "," << m.name << ","
          << crystal_structure_name(m.structure) << ","
          << r.n_beads << "," << (r.converged ? "true" : "false") << ","
          << r.steps_taken << ","
          << std::fixed << std::setprecision(6) << r.final_rms_force << ","
          << r.final_avg_eta << "," << r.final_avg_rho << "," << r.final_avg_C << ","
          << r.final_energy << ","
          << std::setprecision(2) << r.elapsed_ms << ","
          << r.n_l3_domains << ","
          << std::setprecision(4) << r.macro_rigidity << ","
          << r.macro_ductility << "," << r.macro_cohesion << ","
          << m.cohesive_energy_ev << "," << m.melting_point_K << ","
          << m.lattice_constant_ang << ","
          << m.bulk_modulus_GPa << "," << m.shear_modulus_GPa << ","
          << m.youngs_modulus_GPa << ","
          << m.displacement_energy_ev << "," << m.mu_mass_100keV_cm2g << ","
          << m.k_edge_keV << "," << m.heat_of_combustion_kJ_g << ","
          << m.ignition_temperature_K << "," << m.primary_oxide << ","
          << m.flame_colour << "\n";
    }
}

void write_alloy_csv(const std::vector<AlloyPairDescriptor>& pairs,
                     const std::string& path)
{
    std::ofstream f(path);
    f << "pair,A,B,structure,sigma_AB,epsilon_AB,delta_r_pct,delta_chi,"
      << "delta_Ecoh_ev,hume_rothery\n";
    for (const auto& p : pairs) {
        f << p.name << "," << p.symbol_A << "," << p.symbol_B << ","
          << p.structure_compatibility << ","
          << std::fixed << std::setprecision(4) << p.sigma_AB_ang << ","
          << p.epsilon_AB_kcal << ","
          << std::setprecision(2) << p.delta_r_frac * 100.0 << ","
          << p.delta_chi << "," << p.delta_Ecoh_ev << ","
          << (p.hume_rothery_soluble ? "true" : "false") << "\n";
    }
}

void write_markdown_report(const std::vector<MetalSimResult>& results,
                            const std::vector<AlloyPairDescriptor>& pairs,
                            const std::string& path)
{
    auto registry = all_metals();

    std::ofstream f(path);
    f << "# VSEPR-SIM: Metals Research Report\n\n";
    f << "Generated by `metal_sim` — FIRE 6+9 atomistic simulation.\n\n";
    f << "**Engine:** SeedBeadStepper (6 SEED + 9 BEAD units)  \n";
    f << "**Registry:** 12 metals (FCC/BCC/HCP), 10 canonical alloy pairs  \n\n";

    f << "---\n\n## Pure Metals\n\n";
    for (const auto& r : results)
        f << format_report_markdown(r);

    if (!pairs.empty()) {
        f << "---\n\n";
        f << format_alloy_table_markdown(pairs);
    }

    // Energetic signatures table
    f << "---\n\n";
    auto sigs = compute_all_energetic_signatures(registry);
    f << format_energetic_table_markdown(sigs);

    // Radiation shielding ranking
    f << "---\n\n## Radiation Shielding Ranking\n\n";
    f << "Score = 0.6 × (μ/μ_Au) + 0.4 × (E_d/E_d_W). A ≥ 0.75, B ≥ 0.50, C ≥ 0.25, D < 0.25\n\n";
    f << "| Metal | Attenuation | Displacement | Combined | Grade |\n";
    f << "|---|---|---|---|---|\n";
    auto scores = rank_shielding(registry);
    for (const auto& s : scores) {
        char row[128];
        std::snprintf(row, sizeof(row), "| %s | %.3f | %.3f | %.3f | %s |\n",
                      s.material.c_str(), s.attenuation_score,
                      s.displacement_score, s.combined_score, s.grade.c_str());
        f << row;
    }
    f << "\n";

    f << "---\n\n*Source: CRC Handbook 105th ed., Kittel 8th ed., Tyson & Miller 1977, NIST XCOM*\n";
}

// ============================================================================
// Interactive REPL
// ============================================================================

static void print_banner() {
    const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
    const char* RESET = "\033[0m";
    std::cout << BOLD
              << "\n╔══════════════════════════════════════════════════════════╗\n"
              << "║          VSEPR-SIM  •  Metals Research Simulator         ║\n"
              << "║     FIRE 6+9 Engine  •  12 Metals  •  10 Alloy Pairs     ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n"
              << RESET;
}

static void print_metal_menu(const std::vector<MetalRecord>& reg) {
    const char* CYAN = "\033[36m"; const char* RESET = "\033[0m";
    std::cout << CYAN << "\n  Available metals:\n" << RESET;
    for (size_t i = 0; i < reg.size(); ++i) {
        const auto& m = reg[i];
        std::cout << "    " << std::setw(3) << (i+1) << ". "
                  << std::setw(4) << m.symbol << " " << std::setw(14) << m.name
                  << "  [" << std::setw(3) << crystal_structure_name(m.structure) << "]"
                  << "  E_coh=" << std::setprecision(2) << std::fixed << m.cohesive_energy_ev << " eV"
                  << "  T_m=" << std::setprecision(0) << m.melting_point_K << " K";
        if (m.is_noble_metal) std::cout << " ◈";
        if (m.is_refractory)  std::cout << " ▲";
        if (m.is_magnetic)    std::cout << " ⊛";
        std::cout << "\n";
    }
    std::cout << "\n  Legend: ◈ = noble  ▲ = refractory  ⊛ = magnetic\n";
}

static void print_repl_help() {
    std::cout <<
        "\n  Commands:\n"
        "    run <SYMBOL> [n_beads]        — run single metal  (default n=64)\n"
        "    alloy <A> <B> [x_B] [n]       — run alloy pair    (default x_B=0.5, n=64)\n"
        "    batch [n_beads]               — run all metals    (default n=64)\n"
        "    alloys                        — show alloy pair table\n"
        "    list                          — show metal menu\n"
        "    info <SYMBOL>                 — show metal data sheet\n"
        "    params <SYMBOL>               — show FIRE parameters for metal\n"
        "    radiation <SYMBOL> [cm] [keV] — X-ray attenuation + displacement (default 0.01 cm, 100 keV)\n"
        "    scatter <SYMBOL> [n_beads]    — Debye scattering pattern (default n=64)\n"
        "    energetic                     — combustion signatures for all metals\n"
        "    shield                        — radiation shielding ranking\n"
        "    dbdemo                        — database architecture demo (Day 40C)\n"
        "    save                          — write metals_report.md + CSVs\n"
        "    help                          — this message\n"
        "    quit / exit                   — exit\n\n";
}

static void print_metal_info(const MetalRecord& m) {
    const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
    const char* RESET = "\033[0m";
    std::cout << BOLD << "\n  " << m.symbol << " — " << m.name << RESET
              << "  [" << crystal_structure_name(m.structure) << "]\n";
    std::cout << CYAN;
    std::cout << "  Z=" << m.Z << "  mass=" << std::fixed << std::setprecision(3) << m.atomic_mass_amu << " amu\n";
    std::cout << "  a₀=" << m.lattice_constant_ang << " Å"
              << "  r_atom=" << m.atomic_radius_ang << " Å"
              << "  CN_bulk=" << m.bulk_CN << "\n";
    std::cout << "  E_coh=" << m.cohesive_energy_ev << " eV/atom"
              << "  γ_surf=" << m.surface_energy_J_m2 << " J/m²\n";
    std::cout << "  T_melt=" << std::setprecision(1) << m.melting_point_K << " K"
              << "  Θ_D=" << m.debye_temperature_K << " K"
              << "  κ=" << m.thermal_conductivity_W_mK << " W/(m·K)\n";
    std::cout << "  B=" << m.bulk_modulus_GPa << " GPa"
              << "  G=" << m.shear_modulus_GPa << " GPa"
              << "  E=" << m.youngs_modulus_GPa << " GPa"
              << "  ν=" << m.poisson_ratio << "\n";
    std::cout << "  χ=" << m.electronegativity_pauling
              << "  φ=" << m.work_function_ev << " eV"
              << "  E_F=" << m.fermi_energy_ev << " eV\n";
    std::cout << "  LJ: σ=" << m.lj_sigma_ang << " Å"
              << "  ε=" << std::setprecision(4) << m.lj_epsilon_kcal << " kcal/mol\n";
    // Radiation interaction data
    std::cout << "  E_d=" << std::setprecision(1) << m.displacement_energy_ev << " eV"
              << "  μ/ρ(100keV)=" << std::setprecision(3) << m.mu_mass_100keV_cm2g << " cm²/g"
              << "  K-edge=" << m.k_edge_keV << " keV\n";
    // Energetic / combustion data
    if (m.heat_of_combustion_kJ_g > 0) {
        std::cout << "  ΔH_comb=" << std::setprecision(2) << m.heat_of_combustion_kJ_g << " kJ/g"
                  << "  T_ign=" << std::setprecision(0) << m.ignition_temperature_K << " K"
                  << "  oxide=" << m.primary_oxide
                  << "  flame=" << m.flame_colour << "\n";
    } else {
        std::cout << "  Combustion: inert (noble metal) — " << m.flame_colour << "\n";
    }
    std::cout << "  Source: " << m.source << "\n";
    std::cout << RESET;
}

static void print_metal_params(const MetalRecord& m) {
    MetalFireParams mfp = params_for_metal(m);
    auto& p = mfp.seed.env_params;
    const char* CYAN = "\033[36m"; const char* RESET = "\033[0m";
    std::cout << CYAN << "\n  FIRE params for " << m.symbol
              << " [" << mfp.class_label << "]\n" << RESET;
    std::cout << "  Rationale: " << mfp.rationale << "\n";
    std::cout << "  τ=" << p.tau
              << "  α=" << p.alpha << "  β=" << p.beta << "\n";
    std::cout << "  γ_steric=" << p.gamma_steric
              << "  γ_elec=" << p.gamma_elec
              << "  γ_disp=" << p.gamma_disp << "\n";
    std::cout << "  dt_init=" << mfp.seed.dt_initial
              << "  dt_max=" << mfp.seed.dt_max
              << "  f_tol=" << mfp.seed.f_tol
              << "  max_steps=" << mfp.seed.max_steps << "\n";
}

int run_repl(const std::vector<MetalRecord>& registry) {
    std::vector<MetalSimResult> session_results;

    print_banner();
    print_repl_help();
    print_metal_menu(registry);

    const char* PROMPT = "\033[1m\033[32mmetal-sim>\033[0m ";
    std::cout << PROMPT;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd.empty()) {
            std::cout << PROMPT;
            continue;
        }

        // ---- run <SYMBOL> [n] ----
        if (cmd == "run") {
            std::string sym; uint32_t n = 64;
            iss >> sym >> n;
            const MetalRecord* m = find_metal(registry, sym);
            if (!m) {
                std::cout << "  Unknown metal: " << sym << "\n";
            } else {
                auto r = run_metal(*m, n, true, true);
                std::cout << format_report_terminal(r);
                session_results.push_back(r);
            }
        }

        // ---- alloy <A> <B> [x_B] [n] ----
        else if (cmd == "alloy") {
            std::string sA, sB; double x_B = 0.5; uint32_t n = 64;
            iss >> sA >> sB >> x_B >> n;
            const MetalRecord* mA = find_metal(registry, sA);
            const MetalRecord* mB = find_metal(registry, sB);
            if (!mA || !mB) {
                std::cout << "  Unknown metal: " << (mA ? sB : sA) << "\n";
            } else {
                auto r = run_alloy(*mA, *mB, x_B, n, true);
                std::cout << format_report_terminal(r);
                session_results.push_back(r);
            }
        }

        // ---- batch [n] ----
        else if (cmd == "batch") {
            uint32_t n = 64; iss >> n;
            std::cout << "\n  Batch sweep: " << registry.size() << " metals, N=" << n << "\n";
            session_results.clear();
            for (const auto& m : registry) {
                auto r = run_metal(m, n, false, true);
                std::cout << "  " << std::setw(4) << m.symbol
                          << "  [" << std::setw(3) << crystal_structure_name(m.structure) << "]"
                          << "  steps=" << std::setw(6) << r.steps_taken
                          << "  η̄=" << std::fixed << std::setprecision(4) << r.final_avg_eta
                          << "  C̄=" << std::setprecision(2) << r.final_avg_C
                          << "  conv=" << (r.converged ? "\033[32mYES\033[0m" : "\033[31mNO\033[0m")
                          << "  L3=" << r.n_l3_domains
                          << "  t=" << std::setprecision(1) << r.elapsed_ms << " ms\n";
                session_results.push_back(r);
            }
            std::cout << "  Done. " << session_results.size() << " results in session.\n";
        }

        // ---- alloys ----
        else if (cmd == "alloys") {
            auto pairs = canonical_alloy_pairs();
            std::cout << format_alloy_table_terminal(pairs);
        }

        // ---- list ----
        else if (cmd == "list") {
            print_metal_menu(registry);
        }

        // ---- info <SYMBOL> ----
        else if (cmd == "info") {
            std::string sym; iss >> sym;
            const MetalRecord* m = find_metal(registry, sym);
            if (!m) std::cout << "  Unknown metal: " << sym << "\n";
            else    print_metal_info(*m);
        }

        // ---- params <SYMBOL> ----
        else if (cmd == "params") {
            std::string sym; iss >> sym;
            const MetalRecord* m = find_metal(registry, sym);
            if (!m) std::cout << "  Unknown metal: " << sym << "\n";
            else    print_metal_params(*m);
        }

        // ---- radiation <SYMBOL> [thickness_cm] [keV] ----
        else if (cmd == "radiation") {
            std::string sym; double thick = 0.01, keV = 100.0;
            iss >> sym >> thick >> keV;
            const MetalRecord* m = find_metal(registry, sym);
            if (!m) {
                std::cout << "  Unknown metal: " << sym << "\n";
            } else {
                double rho = estimate_bulk_density(*m);
                auto att = compute_attenuation(*m, rho, thick, keV);
                auto dis = compute_displacement(*m, 64, 1000.0);
                auto sc  = compute_shielding_score(*m);

                const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
                const char* GREEN = "\033[32m"; const char* YELLOW = "\033[33m";
                const char* RESET = "\033[0m";
                std::cout << BOLD << "\n  Radiation Profile: " << m->symbol
                          << " (Z=" << m->Z << ")" << RESET << "\n";
                std::cout << CYAN;
                std::cout << "  ── X-ray Attenuation (Beer–Lambert) ──\n";
                std::cout << "  E_photon=" << std::fixed << std::setprecision(1) << keV << " keV"
                          << "  thickness=" << thick << " cm"
                          << "  ρ=" << std::setprecision(2) << rho << " g/cm³\n";
                std::cout << "  μ/ρ=" << std::setprecision(4) << att.mu_mass_cm2g << " cm²/g"
                          << "  μ_linear=" << att.mu_linear_cm << " cm⁻¹\n";
                std::cout << "  Transmission=" << std::setprecision(4) << att.transmission * 100.0 << "%"
                          << "  Attenuation=" << std::setprecision(2) << att.attenuation_pct << "%\n";
                std::cout << "  HVL=" << std::setprecision(4) << att.half_value_layer_cm << " cm"
                          << "  TVL=" << att.tenth_value_layer_cm << " cm";
                if (att.above_k_edge) std::cout << YELLOW << "  [above K-edge]" << RESET << CYAN;
                std::cout << "\n";
                std::cout << "  ── Displacement Damage (Kinchin–Pease) ──\n";
                std::cout << "  E_d=" << std::setprecision(1) << dis.E_d_eV << " eV"
                          << "  T_damage=" << dis.T_damage_eV << " eV\n";
                std::cout << "  N_d=" << std::setprecision(1) << dis.N_displacements
                          << " displacements/PKA"
                          << "  hardness=" << std::setprecision(2) << dis.hardness_factor << "×Cu\n";
                if (dis.displacement_resistant)
                    std::cout << GREEN << "  ✓ Displacement-resistant (E_d > 40 eV)" << RESET << CYAN << "\n";
                std::cout << "  ── Shielding Score ──\n";
                std::cout << "  Attenuation score=" << std::setprecision(3) << sc.attenuation_score
                          << "  Displacement score=" << sc.displacement_score
                          << "  Combined=" << sc.combined_score
                          << "  Grade=" << sc.grade << "\n";
                std::cout << RESET;
            }
        }

        // ---- scatter <SYMBOL> [n_beads] ----
        else if (cmd == "scatter") {
            std::string sym; uint32_t n = 64;
            iss >> sym >> n;
            const MetalRecord* m = find_metal(registry, sym);
            if (!m) {
                std::cout << "  Unknown metal: " << sym << "\n";
            } else {
                BeadSystem sys = build_metal_bead_system(*m, n);
                auto prof = compute_debye_pattern(sys, *m);
                std::cout << scattering_summary_terminal(prof);

                // Write CSV
                std::string csvpath = "scatter_" + m->symbol + ".csv";
                std::ofstream fout(csvpath);
                fout << scattering_to_csv(prof);
                std::cout << "  Written: " << csvpath << " (" << prof.n_q_bins << " bins)\n";
            }
        }

        // ---- energetic ----
        else if (cmd == "energetic") {
            auto sigs = compute_all_energetic_signatures(registry);
            std::cout << format_energetic_table_terminal(sigs);
        }

        // ---- shield ----
        else if (cmd == "shield") {
            auto scores = rank_shielding(registry);
            const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
            const char* GREEN = "\033[32m"; const char* YELLOW = "\033[33m";
            const char* RED = "\033[31m"; const char* RESET = "\033[0m";
            std::cout << BOLD << "\n  Radiation Shielding Ranking (all metals)\n" << RESET;
            std::cout << "  " << std::string(72, '-') << "\n";
            char hdr[128];
            std::snprintf(hdr, sizeof(hdr), "  %-6s %-10s %-14s %-10s %-8s\n",
                          "Metal", "Atten", "Displacement", "Combined", "Grade");
            std::cout << hdr;
            std::cout << "  " << std::string(72, '-') << "\n";
            for (const auto& s : scores) {
                const char* gc = RESET;
                if (s.grade == "A") gc = GREEN;
                else if (s.grade == "B") gc = YELLOW;
                else if (s.grade == "C") gc = RED;
                char row[128];
                std::snprintf(row, sizeof(row), "  %-6s %8.3f   %12.3f   %8.3f   %s%s%s\n",
                              s.material.c_str(), s.attenuation_score,
                              s.displacement_score, s.combined_score,
                              gc, s.grade.c_str(), RESET);
                std::cout << row;
            }
            std::cout << "\n  Score = 0.6×(μ/μ_Au) + 0.4×(E_d/E_d_W).  A≥0.75 B≥0.50 C≥0.25 D<0.25\n\n";
        }

        // ---- graph <SYMBOL> [n_beads] ----
        else if (cmd == "graph") {
            std::string sym; uint32_t n = 64;
            iss >> sym >> n;
            const MetalRecord* m = find_metal(registry, sym);
            if (!m) {
                std::cout << "  Unknown metal: " << sym << "\n";
            } else {
                BeadSystem sys = build_metal_bead_system(*m, n);
                double r_cut = m->lattice_constant_ang * 2.0;
                auto G = coarse_grain::theory::build_graph(sys, r_cut, 1.0, true);
                auto diag = coarse_grain::theory::diagnose_graph(G);

                const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
                const char* RESET = "\033[0m";
                std::cout << BOLD << "\n  Interaction Graph: " << m->symbol
                          << "  G=(V,E)" << RESET << "\n";
                std::cout << CYAN;
                std::cout << "  |V| = " << diag.N << "  (beads)\n";
                std::cout << "  |E| = " << diag.num_edges << "  (edges within r_cut="
                          << std::fixed << std::setprecision(2) << r_cut << " \u00c5)\n";
                std::cout << "  <k> = " << std::setprecision(2) << diag.mean_degree
                          << "  max(k) = " << diag.max_degree
                          << "  Var(k) = " << std::setprecision(3) << diag.degree_variance << "\n";
                std::cout << "  <r_ij> = " << std::setprecision(3) << diag.mean_separation << " \u00c5"
                          << "  min = " << diag.min_separation
                          << "  max = " << diag.max_separation << " \u00c5\n";
                std::cout << "  |E|/|V| = " << std::setprecision(2) << diag.density_edges_per_v << "\n";

                // Degree distribution summary
                std::vector<int> deg_hist(diag.max_degree + 1, 0);
                for (uint32_t k = 0; k < G.N; ++k)
                    deg_hist[G.degree[k]]++;
                std::cout << "  Degree distribution:";
                for (uint32_t d = 0; d <= diag.max_degree; ++d) {
                    if (deg_hist[d] > 0)
                        std::cout << "  k=" << d << ":" << deg_hist[d];
                }
                std::cout << "\n";

                // Adjacency verification: deg(i) = \u03a3 A_ij
                std::cout << "  Trace(L) = 2|E| = " << 2 * diag.num_edges
                          << "  (\u03a3 deg(i) = " << 2 * diag.num_edges << " \u2713)\n";
                std::cout << RESET;
            }
        }

        // ---- kernel <SYMBOL> [n_beads] ----
        else if (cmd == "kernel") {
            std::string sym; uint32_t n = 64;
            iss >> sym >> n;
            const MetalRecord* m = find_metal(registry, sym);
            if (!m) {
                std::cout << "  Unknown metal: " << sym << "\n";
            } else {
                // Run a quick simulation to get converged env states
                BeadSystem sys = build_metal_bead_system(*m, n);
                MetalFireParams mfp = params_for_metal(*m);
                SeedBeadParams& params = mfp.seed;
                const size_t N = sys.beads.size();
                std::vector<EnvironmentState> env_states(N);
                std::vector<atomistic::Vec3> velocities(N, {0,0,0});
                std::vector<atomistic::Vec3> forces(N, {0,0,0});
                FIREState fire;
                fire.dt = params.dt_initial;
                fire.alpha = params.fire_alpha_start;
                SeedBeadStepper::init(sys, env_states, params);

                double final_energy = 0.0;
                for (uint64_t s = 0; s < params.max_steps; ++s) {
                    auto rec = SeedBeadStepper::step(
                        sys, env_states, velocities, forces, fire, params, s);
                    if (rec.steady_state) {
                        final_energy = rec.total_energy;
                        break;
                    }
                    final_energy = rec.total_energy;
                }

                // Build material kernels from converged state
                std::vector<coarse_grain::theory::MaterialKernel> kernels(N);
                double E_per_bead = final_energy / static_cast<double>(N);
                for (size_t i = 0; i < N; ++i) {
                    kernels[i] = coarse_grain::theory::build_kernel_from_metal(
                        static_cast<uint32_t>(i), env_states[i],
                        m->electronegativity_pauling,
                        m->work_function_ev, E_per_bead);
                }

                auto spec = coarse_grain::theory::compute_kernel_spectrum(kernels);

                const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
                const char* RESET = "\033[0m";
                std::cout << BOLD << "\n  Material Kernel Spectrum: " << m->symbol
                          << "  M_k = [\u03c6, \u03c8, \u03c7, \u03c9, E]\u1d40" << RESET << "\n";
                std::cout << CYAN;
                std::cout << "  N_valid = " << spec.N << " beads\n";
                std::cout << "  <M_k>:  \u03c6=" << std::fixed << std::setprecision(4) << spec.mean.phi
                          << "  \u03c8=" << spec.mean.psi
                          << "  \u03c7=" << spec.mean.chi
                          << "  \u03c9=" << spec.mean.omega
                          << "  E=" << spec.mean.E << "\n";
                std::cout << "  Var:   \u03c6=" << std::scientific << std::setprecision(3) << spec.variance.phi
                          << "  \u03c8=" << spec.variance.psi
                          << "  \u03c7=" << spec.variance.chi
                          << "  \u03c9=" << spec.variance.omega
                          << "  E=" << spec.variance.E << "\n";
                std::cout << "  <||M||> = " << std::fixed << std::setprecision(4) << spec.mean_norm
                          << "  Var(||M||) = " << std::scientific << std::setprecision(3) << spec.norm_variance << "\n";
                std::cout << RESET;
            }
        }

        // ---- save ----
        else if (cmd == "dbdemo") {
            std::cout << vsepr::database::demo::full_demo_report() << "\n";
        }

        else if (cmd == "save") {
            if (session_results.empty()) {
                std::cout << "  No results in session — run something first.\n";
            } else {
                auto pairs = canonical_alloy_pairs();
                write_results_csv(session_results, "metals_results.csv");
                write_alloy_csv(pairs, "alloy_pairs.csv");
                write_markdown_report(session_results, pairs, "metals_report.md");
                std::cout << "  Written: metals_report.md\n";
                std::cout << "  Written: metals_results.csv (" << session_results.size() << " rows)\n";
                std::cout << "  Written: alloy_pairs.csv (" << pairs.size() << " pairs)\n";
            }
        }

        else if (cmd == "help") {
            print_repl_help();
        }

        else if (cmd == "quit" || cmd == "exit" || cmd == "q") {
            std::cout << "\n  Exiting metals simulator.\n\n";
            break;
        }

        else {
            std::cout << "  Unknown command: " << cmd << " — type 'help'\n";
        }

        std::cout << PROMPT;
    }

    // Auto-save if we have results
    if (!session_results.empty()) {
        auto pairs = canonical_alloy_pairs();
        write_results_csv(session_results, "metals_results.csv");
        write_alloy_csv(pairs, "alloy_pairs.csv");
        write_markdown_report(session_results, pairs, "metals_report.md");
        std::cout << "\n  Session saved: metals_report.md, metals_results.csv\n";
    }

    return 0;
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[]) {
    auto registry = all_metals();

    // ---- No args → interactive REPL ----
    if (argc == 1) {
        return run_repl(registry);
    }

    std::string arg1 = argv[1];

    // ---- --batch ----
    if (arg1 == "--batch") {
        uint32_t n = (argc >= 3) ? static_cast<uint32_t>(std::atoi(argv[2])) : 64;
        print_banner();
        std::cout << "\n  Batch sweep: " << registry.size() << " metals, N=" << n << "\n";
        std::vector<MetalSimResult> results;
        for (const auto& m : registry) {
            auto r = run_metal(m, n, false, true);
            std::cout << format_report_terminal(r);
            results.push_back(r);
        }
        auto pairs = canonical_alloy_pairs();
        write_results_csv(results, "metals_results.csv");
        write_alloy_csv(pairs, "alloy_pairs.csv");
        write_markdown_report(results, pairs, "metals_report.md");
        std::cout << "\n  Written: metals_report.md  metals_results.csv  alloy_pairs.csv\n";
        return 0;
    }

    // ---- --alloys ----
    if (arg1 == "--alloys") {
        auto pairs = canonical_alloy_pairs();
        std::cout << format_alloy_table_terminal(pairs);
        write_alloy_csv(pairs, "alloy_pairs.csv");
        std::cout << "  Written: alloy_pairs.csv\n";
        return 0;
    }

    // ---- <SYMBOL> [n] ----
    if (argc >= 2 && argc <= 3) {
        uint32_t n = (argc == 3) ? static_cast<uint32_t>(std::atoi(argv[2])) : 64;
        const MetalRecord* m = find_metal(registry, arg1);
        if (!m) {
            std::cerr << "  Unknown metal symbol: " << arg1 << "\n";
            std::cerr << "  Available: Au Ag Cu Pt Ni Al Fe W Mo Cr Ti Co\n";
            return 1;
        }
        print_banner();
        auto r = run_metal(*m, n, true, true);
        std::cout << format_report_terminal(r);
        auto pairs = canonical_alloy_pairs();
        write_results_csv({r}, "metals_results.csv");
        write_markdown_report({r}, {}, "metals_report.md");
        return 0;
    }

    // ---- <A> <B> [x_B] [n] ----
    if (argc >= 3) {
        std::string symA = arg1, symB = argv[2];
        double x_B = (argc >= 4) ? std::atof(argv[3]) : 0.5;
        uint32_t n = (argc >= 5) ? static_cast<uint32_t>(std::atoi(argv[4])) : 64;

        const MetalRecord* mA = find_metal(registry, symA);
        const MetalRecord* mB = find_metal(registry, symB);
        if (!mA || !mB) {
            std::cerr << "  Unknown metal: " << (!mA ? symA : symB) << "\n";
            return 1;
        }
        print_banner();
        auto r = run_alloy(*mA, *mB, x_B, n, true);
        AlloyPairDescriptor ap = make_alloy_pair(*mA, *mB);
        std::cout << format_report_terminal(r);
        std::cout << format_alloy_table_terminal({ap});
        write_results_csv({r}, "metals_results.csv");
        write_alloy_csv({ap}, "alloy_pairs.csv");
        write_markdown_report({r}, {ap}, "metals_report.md");
        return 0;
    }

    std::cerr << "  Usage: metal_sim [--batch|--alloys|SYMBOL [n]|A B [x_B] [n]]\n";
    return 1;
}
