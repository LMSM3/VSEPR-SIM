/**
 * fire_smooth_task.cpp — FIRE Smooth-Sampling Experimental Task
 *
 * Experimental runner that extends the FIRE 6+9 relaxation with:
 *   - Smooth random sampling of arrangement parameters (Gaussian perturbations)
 *   - QM descriptor computation (Level-0 analytic) for each converged state
 *   - Level 3 Bead aggregation (L2 → L3 domain clustering)
 *   - Macro-DM precursor handoff records (rigidity*, ductility*, transport*...)
 *   - Full per-step trace attached to seed hash
 *
 * This is the integration test for three new infrastructure layers:
 *   coarse_grain/fire_smooth/   — smooth sampler + runner
 *   coarse_grain/qm/            — QM descriptor preparation (Level 0)
 *   coarse_grain/level3/        — Level 3 Beading, macro-DM handoff
 *
 * Usage:
 *   fire-smooth [master_seed] [n_samples]
 *   defaults: seed=42, n_samples=200
 *
 * Outputs:
 *   fire_smooth_results.csv       — one row per sample, all metadata
 *   fire_smooth_l3.csv            — one row per L3 domain
 *   fire_smooth_trace/<HASH>.csv  — per-step trace per sample
 *   fire_smooth_report.md         — summary report
 *
 * Anti-black-box: every perturbation, QM value, and L3 aggregate
 * is explicitly recorded and traceable to its seed hash.
 */

#include "coarse_grain/fire_smooth/fire_smooth_runner.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>

using namespace coarse_grain;
using namespace coarse_grain::fire_smooth;
using namespace coarse_grain::chemistry;

// ============================================================================
// Arrangement table (shared with sim_1000)
// ============================================================================

struct ArrangementDescriptor {
    uint32_t    id{};
    std::string label;
    std::string base_reaction;
    double      temperature_K{298.15};
    double      tau{100.0};
    double      gamma_steric{0.2};
    double      gamma_elec{-0.15};
    double      gamma_disp{0.45};
};

static ChemicalReaction get_base_reaction(const std::string& name)
{
    if (name == "benzene_nitration")    return library::build_benzene_nitration();
    if (name == "thorium_oxalate")      return library::build_thorium_oxalate_precipitation();
    if (name == "copper_decomposition") return library::build_copper_nitrate_decomposition();
    return library::build_benzene_nitration();
}

static std::vector<ArrangementDescriptor> build_arrangements()
{
    std::vector<ArrangementDescriptor> arr;
    arr.reserve(50);

    struct Base { std::string name; double tau; double gs; double ge; double gd; };
    std::vector<Base> bases = {
        {"benzene_nitration",    60.0,  0.20, -0.15,  0.45},
        {"thorium_oxalate",      40.0,  0.25, -0.20,  0.30},
        {"copper_decomposition", 30.0,  0.30, -0.10,  0.55},
    };
    double temps[] = {250.0, 298.15, 500.0};
    const char* ttags[] = {"cold", "std", "hot"};
    double csc[] = {0.5, 1.0, 2.0};
    const char* ctags[] = {"weak", "mod", "strong"};

    uint32_t id = 0;
    for (auto& b : bases) {
        for (int ti = 0; ti < 3 && id < 50; ++ti) {
            for (int ci = 0; ci < 3 && id < 50; ++ci) {
                ArrangementDescriptor ad;
                ad.id           = id++;
                ad.base_reaction   = b.name;
                ad.temperature_K   = temps[ti];
                ad.tau             = b.tau    * csc[ci];
                ad.gamma_steric    = b.gs     * csc[ci];
                ad.gamma_elec      = b.ge     * csc[ci];
                ad.gamma_disp      = b.gd     * csc[ci];
                ad.label = b.name + "_" + ttags[ti] + "_" + ctags[ci];
                arr.push_back(ad);
            }
        }
    }
    while (arr.size() < 50) {
        auto ad = arr.back();
        ad.id = static_cast<uint32_t>(arr.size());
        ad.temperature_K += 25.0;
        ad.label += "_ext";
        arr.push_back(ad);
    }
    return arr;
}

// ============================================================================
// Write per-sample trace CSV
// ============================================================================

static void write_trace(const std::string& dir, const SmoothSampleRecord& rec)
{
    std::ofstream f(dir + "/" + rec.descriptor.seed_hash + ".csv");
    if (!f.is_open()) return;

    const auto& d = rec.descriptor;
    f << "# seed_hash: "       << d.seed_hash         << "\n";
    f << "# sample_index: "    << d.sample_index       << "\n";
    f << "# arrangement: "     << d.arrangement_label  << "\n";
    f << "# equation: "        << d.equation           << "\n";
    f << "# tau_drawn: "       << d.tau_drawn          << "\n";
    f << "# g_steric_drawn: "  << d.gamma_steric_drawn << "\n";
    f << "# g_elec_drawn: "    << d.gamma_elec_drawn   << "\n";
    f << "# g_disp_drawn: "    << d.gamma_disp_drawn   << "\n";
    f << "# temperature_K: "   << d.temperature_drawn  << "\n";
    f << "# dt_drawn: "        << d.dt_drawn           << "\n";
    f << "# N_beads: "         << d.N_beads            << "\n";
    f << "# total_mass_amu: "  << d.total_mass_amu     << "\n";
    f << "#\n";
    f << "step,energy,ke,rms_force,max_force,dt,"
         "avg_rho,avg_C,avg_P2,avg_eta,avg_target_f,max_deta,"
         "g_steric,g_elec,g_disp,"
         "n_inert,n_ionic,n_cov,n_met,n_mix,steady\n";

    f << std::fixed;
    for (const auto& t : rec.trace) {
        f << t.step_index
          << "," << std::setprecision(8) << t.total_energy
          << "," << std::setprecision(8) << t.kinetic_energy
          << "," << std::setprecision(8) << t.rms_force
          << "," << std::setprecision(8) << t.max_force
          << "," << std::setprecision(6) << t.dt_current
          << "," << std::setprecision(8) << t.avg_rho
          << "," << std::setprecision(8) << t.avg_C
          << "," << std::setprecision(8) << t.avg_P2
          << "," << std::setprecision(8) << t.avg_eta
          << "," << std::setprecision(8) << t.avg_target_f
          << "," << std::setprecision(8) << t.max_delta_eta
          << "," << std::setprecision(8) << t.avg_g_steric
          << "," << std::setprecision(8) << t.avg_g_elec
          << "," << std::setprecision(8) << t.avg_g_disp
          << "," << t.n_inert << "," << t.n_ionic
          << "," << t.n_covalent << "," << t.n_metallic << "," << t.n_mixed
          << "," << (t.steady_state ? 1 : 0) << "\n";
    }
}

// ============================================================================
// Write results CSV
// ============================================================================

static void write_results_csv(const std::string& path,
                               const std::vector<SmoothSampleRecord>& recs)
{
    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "ERROR: cannot write " << path << "\n"; return; }

    f << "sample_index,seed_hash,master_seed,arrangement_id,arrangement_label,"
         "base_reaction,equation,temperature_drawn,tau_drawn,"
         "g_steric_drawn,g_elec_drawn,g_disp_drawn,dt_drawn,"
         "z_tau,z_g_steric,z_g_elec,z_g_disp,z_temperature,z_dt,"
         "N_beads,total_mass_amu,"
         "converged,steps_taken,final_energy,final_rms_force,final_avg_eta,"
         "n_l3_domains,elapsed_ms\n";

    f << std::fixed;
    for (const auto& r : recs) {
        const auto& d = r.descriptor;
        f << d.sample_index             << ","
          << d.seed_hash                << ","
          << d.master_seed              << ","
          << d.arrangement_id           << ","
          << d.arrangement_label        << ","
          << d.base_reaction            << ","
          << "\"" << d.equation << "\"" << ","
          << std::setprecision(2) << d.temperature_drawn << ","
          << std::setprecision(4) << d.tau_drawn         << ","
          << std::setprecision(4) << d.gamma_steric_drawn << ","
          << std::setprecision(4) << d.gamma_elec_drawn   << ","
          << std::setprecision(4) << d.gamma_disp_drawn   << ","
          << std::setprecision(4) << d.dt_drawn           << ","
          << std::setprecision(4) << d.z_tau              << ","
          << std::setprecision(4) << d.z_gamma_steric     << ","
          << std::setprecision(4) << d.z_gamma_elec       << ","
          << std::setprecision(4) << d.z_gamma_disp       << ","
          << std::setprecision(4) << d.z_temperature      << ","
          << std::setprecision(4) << d.z_dt               << ","
          << d.N_beads                  << ","
          << std::setprecision(4) << d.total_mass_amu     << ","
          << (r.converged ? "true" : "false") << ","
          << r.steps_taken              << ","
          << std::setprecision(8) << r.final_energy       << ","
          << std::setprecision(8) << r.final_rms_force    << ","
          << std::setprecision(8) << r.final_avg_eta      << ","
          << r.l3_domains.size()        << ","
          << std::setprecision(1) << r.elapsed_ms         << "\n";
    }
    std::cout << "  Written: " << path << " (" << recs.size() << " rows)\n";
}

// ============================================================================
// Write L3 domains CSV
// ============================================================================

static void write_l3_csv(const std::string& path,
                          const std::vector<SmoothSampleRecord>& recs)
{
    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "ERROR: cannot write " << path << "\n"; return; }

    f << "seed_hash,sample_index,domain_id,n_members,valid,"
         "pos_x,pos_y,pos_z,radius,volume,"
         "rho_eff,charge_density,phi_eff,polarisability,chi_eff,"
         "mean_eta,mean_rho,mean_C,mean_P2,var_eta,"
         "qm_phi,qm_chi,qm_alpha,qm_homo,qm_lumo,qm_gap,qm_q_eff,qm_overlap,"
         "rigidity_like,ductility_like,brittleness_like,"
         "thermal_transport_like,electrical_transport_like,"
         "surface_reactivity_like,macro_valid\n";

    f << std::fixed << std::setprecision(6);
    for (const auto& r : recs) {
        for (const auto& l3 : r.l3_domains) {
            const auto& ms = l3.macro_state;
            f << r.descriptor.seed_hash  << ","
              << r.descriptor.sample_index << ","
              << l3.domain_id            << ","
              << l3.n_members            << ","
              << (l3.valid ? 1 : 0)      << ","
              << l3.position_com.x       << ","
              << l3.position_com.y       << ","
              << l3.position_com.z       << ","
              << l3.radius               << ","
              << l3.volume               << ","
              << l3.rho_eff              << ","
              << l3.charge_density       << ","
              << l3.phi_eff              << ","
              << l3.polarisability       << ","
              << l3.chi_eff              << ","
              << l3.mean_eta             << ","
              << l3.mean_rho             << ","
              << l3.mean_C               << ","
              << l3.mean_P2              << ","
              << l3.var_eta              << ","
              << l3.qm.phi_elec          << ","
              << l3.qm.chi_mean          << ","
              << l3.qm.alpha_proxy       << ","
              << l3.qm.homo_proxy        << ","
              << l3.qm.lumo_proxy        << ","
              << l3.qm.chemical_gap      << ","
              << l3.qm.q_eff             << ","
              << l3.qm.omega_overlap     << ","
              << (ms.rigidity_like.valid          ? ms.rigidity_like.value          : -1.0) << ","
              << (ms.ductility_like.valid         ? ms.ductility_like.value         : -1.0) << ","
              << (ms.brittleness_like.valid       ? ms.brittleness_like.value       : -1.0) << ","
              << (ms.thermal_transport_like.valid ? ms.thermal_transport_like.value : -1.0) << ","
              << (ms.electrical_transport_like.valid ? ms.electrical_transport_like.value : -1.0) << ","
              << (ms.surface_reactivity_like.valid ? ms.surface_reactivity_like.value : -1.0) << ","
              << (ms.valid ? 1 : 0) << "\n";
        }
    }

    uint32_t total_l3 = 0;
    for (const auto& r : recs) total_l3 += static_cast<uint32_t>(r.l3_domains.size());
    std::cout << "  Written: " << path << " (" << total_l3 << " L3 domain rows)\n";
}

// ============================================================================
// Write Markdown report
// ============================================================================

static void write_report(const std::string& path,
                          uint64_t master_seed,
                          uint32_t n_samples,
                          const std::vector<SmoothSampleRecord>& recs)
{
    std::ofstream f(path);
    if (!f.is_open()) return;

    uint32_t n_conv = 0;
    double total_ms = 0.0;
    uint64_t total_steps = 0;
    uint32_t total_l3 = 0;
    uint32_t valid_l3 = 0;
    for (const auto& r : recs) {
        if (r.converged) ++n_conv;
        total_ms    += r.elapsed_ms;
        total_steps += r.steps_taken;
        total_l3    += static_cast<uint32_t>(r.l3_domains.size());
        for (const auto& l3 : r.l3_domains) if (l3.valid) ++valid_l3;
    }

    f << "# VSEPR-SIM: FIRE Smooth-Sampling Task\n\n";
    f << "**Experimental layer: `fire_smooth` + `qm` + `level3`**\n\n";
    f << "---\n\n";

    f << "## 1. Run Configuration\n\n";
    f << "| Parameter | Value |\n|-----------|-------|\n";
    f << "| Master seed | `" << master_seed << "` |\n";
    f << "| Samples | " << n_samples << " |\n";
    f << "| Perturbation model | Gaussian (log-normal τ, additive γ channels) |\n";
    f << "| QM fidelity | Level-0 analytic (φ, χ, α, HOMO/LUMO proxy, q_eff, Ω) |\n";
    f << "| L3 clustering | Greedy nearest-neighbour, r_domain=15Å, min_members=4 |\n";
    f << "| Macro-DM channels | rigidity*, ductility*, brittleness*, transport*, reactivity* |\n\n";

    f << "## 2. Convergence\n\n";
    f << "| Metric | Value |\n|--------|-------|\n";
    f << std::fixed << std::setprecision(1);
    f << "| Converged | " << n_conv << " / " << recs.size() << " ("
      << (recs.empty() ? 0.0 : 100.0 * n_conv / recs.size()) << "%) |\n";
    f << "| Total wall time | " << total_ms / 1000.0 << " s |\n";
    f << "| Avg time / sample | " << (recs.empty() ? 0.0 : total_ms / recs.size()) << " ms |\n";
    f << "| Total integration steps | " << total_steps << " |\n\n";

    f << "## 3. Level 3 Bead Aggregation\n\n";
    f << "| Metric | Value |\n|--------|-------|\n";
    f << "| Total L3 domains formed | " << total_l3 << " |\n";
    f << "| Valid L3 domains (≥4 members, all converged) | " << valid_l3 << " |\n";
    f << std::setprecision(2);
    f << "| L3 domains / sample (avg) | "
      << (recs.empty() ? 0.0 : (double)total_l3 / recs.size()) << " |\n\n";

    f << "## 4. Perturbation Distribution (first 20 samples)\n\n";
    f << "| # | Hash | Arr | T_drawn | τ_drawn | γ_e_drawn | Conv | L3 |\n";
    f << "|---|------|-----|---------|---------|-----------|------|----|\n";
    uint32_t show = std::min((uint32_t)recs.size(), 20u);
    for (uint32_t i = 0; i < show; ++i) {
        const auto& r = recs[i];
        const auto& d = r.descriptor;
        f << "| " << d.sample_index
          << " | `" << d.seed_hash.substr(8) << "`"
          << " | " << d.arrangement_id
          << " | " << std::setprecision(1) << d.temperature_drawn
          << " | " << std::setprecision(1) << d.tau_drawn
          << " | " << std::setprecision(3) << d.gamma_elec_drawn
          << " | " << (r.converged ? "YES" : "no")
          << " | " << r.l3_domains.size() << " |\n";
    }
    f << "\n";

    f << "## 5. New Infrastructure Layers\n\n";
    f << "| Layer | Directory | Status |\n";
    f << "|-------|-----------|--------|\n";
    f << "| FIRE Smooth Sampler | `coarse_grain/fire_smooth/` | Active |\n";
    f << "| QM Descriptors (L0) | `coarse_grain/qm/` | Active (analytic) |\n";
    f << "| Level 3 Beading | `coarse_grain/level3/` | Active |\n";
    f << "| Macro-DM Precursors | `coarse_grain/analysis/macro_precursor.hpp` | Wired |\n\n";

    f << "---\n";
    f << "*VSEPR-SIM v2.9.2 — deterministic atomistic platform*\n";
    f << "*fire_smooth: experimental FIRE + smooth sampling + QM + L3 Beading*\n";

    std::cout << "  Written: " << path << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    uint64_t master_seed = 42ULL;
    uint32_t n_samples   = 200;

    if (argc >= 2) { try { master_seed = std::stoull(argv[1]); } catch (...) {} }
    if (argc >= 3) { try { n_samples   = static_cast<uint32_t>(std::stoul(argv[2])); } catch (...) {} }

    std::cout << "================================================================\n";
    std::cout << " VSEPR-SIM: FIRE Smooth-Sampling Task (Experimental)\n";
    std::cout << " Master seed: " << master_seed
              << "  Samples: " << n_samples << "\n";
    std::cout << " Layers: fire_smooth | qm (L0) | level3 | macro-DM\n";
    std::cout << "================================================================\n\n";

    auto arrangements = build_arrangements();
    std::cout << "Arrangements: " << arrangements.size() << "\n";

    std::mt19937_64 rng(master_seed);
    std::uniform_int_distribution<uint32_t> arr_dist(
        0, static_cast<uint32_t>(arrangements.size() - 1));

    SmoothPerturbParams perturb;  // default widths
    const uint64_t max_steps = 500;

    std::filesystem::create_directories("fire_smooth_trace");

    std::vector<SmoothSampleRecord> all_results;
    all_results.reserve(n_samples);

    auto wall_t0 = std::chrono::high_resolution_clock::now();

    std::cout << "Running " << n_samples << " smooth samples...\n";
    std::cout << std::string(64, '-') << "\n";

    for (uint32_t i = 0; i < n_samples; ++i) {
        uint32_t arr_idx     = arr_dist(rng);
        const auto& ad       = arrangements[arr_idx];
        auto rxn             = get_base_reaction(ad.base_reaction);

        // Construct equation string
        std::string equation = rxn.equation;

        // Draw sample descriptor
        auto desc = draw_sample(
            i, master_seed, ad.id, ad.label,
            ad.base_reaction, equation,
            ad.tau, ad.gamma_steric, ad.gamma_elec, ad.gamma_disp,
            ad.temperature_K, 0.5,
            perturb, rng);

        // Progress every 25 samples
        if (i % 25 == 0) {
            std::cout << "[" << std::setw(4) << i << "/" << n_samples << "] "
                      << "arr=" << std::setw(2) << arr_idx
                      << " hash=" << desc.seed_hash
                      << " T=" << std::fixed << std::setprecision(1) << desc.temperature_drawn
                      << "K tau=" << std::setprecision(1) << desc.tau_drawn << "\n";
        }

        // Run FIRE relaxation + L3 aggregation
        auto result = run_smooth_sample(desc, rxn, max_steps);

        // Write per-sample trace
        write_trace("fire_smooth_trace", result);

        // Clear trace from memory after writing (disk only)
        result.trace.clear();
        result.trace.shrink_to_fit();

        all_results.push_back(std::move(result));
    }

    auto wall_t1 = std::chrono::high_resolution_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_t1 - wall_t0).count();

    std::cout << std::string(64, '-') << "\n";

    uint32_t n_conv = 0;
    uint32_t total_l3 = 0;
    for (const auto& r : all_results) {
        if (r.converged) ++n_conv;
        total_l3 += static_cast<uint32_t>(r.l3_domains.size());
    }

    std::cout << "Completed " << n_samples << " samples in "
              << std::fixed << std::setprecision(2) << wall_ms / 1000.0 << " s\n";
    std::cout << "Converged: " << n_conv << " / " << n_samples << "\n";
    std::cout << "L3 domains formed: " << total_l3 << "\n\n";

    std::cout << "Writing outputs...\n";
    write_results_csv("fire_smooth_results.csv",  all_results);
    write_l3_csv     ("fire_smooth_l3.csv",        all_results);
    write_report     ("fire_smooth_report.md",
                      master_seed, n_samples, all_results);

    std::cout << "\nTrace files: fire_smooth_trace/ ("
              << n_samples << " files)\n";
    std::cout << "\n================================================================\n";
    std::cout << " Done. New layers active:\n";
    std::cout << "   coarse_grain/fire_smooth/  — smooth sampler\n";
    std::cout << "   coarse_grain/qm/           — QM descriptors (Level 0)\n";
    std::cout << "   coarse_grain/level3/       — L3 Beading, macro-DM handoff\n";
    std::cout << "================================================================\n";

    return 0;
}
