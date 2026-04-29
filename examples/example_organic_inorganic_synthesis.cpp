/**
 * example_organic_inorganic_synthesis.cpp
 *
 * Example 1: Organic–Inorganic Synthesis Study
 * ==============================================
 *
 * Models ethanol (C₂H₅OH) adsorption onto a gold (Au) nanoparticle surface.
 *
 * This is a multi-species system with two bead types:
 *   Type 0 — Au (metal substrate):  σ = 2.629 Å, ε = 5.29 kcal/mol
 *   Type 1 — EtOH (organic adsorbate): σ = 3.70 Å, ε = 0.44 kcal/mol
 *
 * Cross-interactions use Lorentz-Berthelot combining rules:
 *   σ_AB = (σ_A + σ_B) / 2
 *   ε_AB = √(ε_A · ε_B)
 *
 * The environment-responsive layer captures:
 *   - Gold beads forming a dense FCC-like core (high ρ, high C, high η)
 *   - Ethanol beads adsorbing at surface sites (lower ρ, lower η)
 *   - Kernel modulation stiffening Au–Au bonds while softening Au–EtOH
 *
 * Outputs:
 *   - Markdown report with comparison metrics
 *   - CSV timeseries and snapshot data
 *   - Excel XML spreadsheet
 *   - SolidWorks .sldcrv and .xyz point cloud
 *
 * Scientific context:
 *   Self-assembled monolayer (SAM) formation of thiolated organics
 *   on gold is fundamental to biosensor fabrication, nanoelectronics,
 *   and surface-enhanced spectroscopy.  This example captures the
 *   essential physics: a dense metal core with a soft organic shell.
 *
 * Usage:
 *   organic-inorganic-synthesis [n_au] [n_ethanol]
 *   (defaults: 27 Au beads, 18 EtOH beads)
 */

#include "coarse_grain/models/seed_bead_stepper.hpp"
#include "coarse_grain/report/snapshot_graph.hpp"
#include "coarse_grain/report/excel_export.hpp"
#include "coarse_grain/report/solidworks_export.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace coarse_grain;

// ============================================================================
// System Builder — Organic-Inorganic Multi-Species
// ============================================================================

struct OrganicInorganicConfig {
    BeadSystem system;
    SeedBeadParams params;
    uint32_t n_au{};
    uint32_t n_ethanol{};
};

/**
 * Build a gold core with ethanol adsorbates arranged around the surface.
 *
 * Gold core: 3×3×3 = 27 beads in simple cubic arrangement
 * Ethanol shell: placed radially outside the gold cluster
 */
OrganicInorganicConfig build_organic_inorganic_system(
    uint32_t n_au = 27,
    uint32_t n_ethanol = 18)
{
    OrganicInorganicConfig cfg;
    cfg.n_au = n_au;
    cfg.n_ethanol = n_ethanol;

    // --- Bead types ---

    // Type 0: Gold (Au)
    // σ = a₀ / 2^(1/6) ≈ 4.078 / 1.122 ≈ 3.63 Å
    // ε = |E_coh| / (CN/2) ≈ 3.81 eV / 6 ≈ 0.635 eV ≈ 14.64 kcal/mol
    // (Scaled down for CG representation)
    BeadType au_type;
    au_type.name = "Au";
    au_type.id = 0;
    au_type.sigma = 2.629;      // LJ σ for Au–Au (CG effective)
    au_type.epsilon = 5.29;     // LJ ε for Au–Au (kcal/mol, CG effective)
    cfg.system.bead_types.push_back(au_type);

    // Type 1: Ethanol (C₂H₅OH) — united-atom CG bead
    // σ ≈ 3.70 Å (TraPPE-UA ethanol)
    // ε ≈ 0.44 kcal/mol (TraPPE-UA)
    BeadType ethanol_type;
    ethanol_type.name = "EtOH";
    ethanol_type.id = 1;
    ethanol_type.sigma = 3.70;      // TraPPE-UA ethanol σ
    ethanol_type.epsilon = 0.44;    // TraPPE-UA ethanol ε (kcal/mol)
    cfg.system.bead_types.push_back(ethanol_type);

    // --- Gold core (simple cubic lattice) ---
    double a_au = 4.078;    // Au lattice constant (Å)
    int n_side = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(n_au))));
    double offset = -0.5 * a_au * (n_side - 1);

    uint32_t count = 0;
    for (int ix = 0; ix < n_side && count < n_au; ++ix) {
        for (int iy = 0; iy < n_side && count < n_au; ++iy) {
            for (int iz = 0; iz < n_side && count < n_au; ++iz) {
                Bead b;
                b.position = {
                    offset + a_au * ix,
                    offset + a_au * iy,
                    offset + a_au * iz
                };
                b.mass = 196.967;   // Au atomic mass (amu)
                b.charge = 0.0;
                b.type_id = 0;      // Au
                cfg.system.beads.push_back(b);
                ++count;
            }
        }
    }

    // --- Ethanol adsorbate shell ---
    // Place ethanol beads on a sphere around the gold cluster
    double core_radius = 0.5 * a_au * n_side + 2.0;  // Just outside the core
    for (uint32_t i = 0; i < n_ethanol; ++i) {
        // Distribute on sphere using golden spiral
        double phi = std::acos(1.0 - 2.0 * (i + 0.5) / n_ethanol);
        double theta = M_PI * (1.0 + std::sqrt(5.0)) * i;

        Bead b;
        b.position = {
            core_radius * std::sin(phi) * std::cos(theta),
            core_radius * std::sin(phi) * std::sin(theta),
            core_radius * std::cos(phi)
        };
        b.mass = 46.069;       // Ethanol molecular mass (amu)
        b.charge = 0.0;
        b.type_id = 1;         // EtOH
        cfg.system.beads.push_back(b);
    }

    cfg.system.source_atom_count = n_au + n_ethanol;

    // --- Parameters tuned for organic-inorganic interface ---
    cfg.params.dt_initial = 1.0;
    cfg.params.dt_max = 8.0;
    cfg.params.f_tol = 0.02;
    cfg.params.e_tol = 1.0e-6;
    cfg.params.max_steps = 3000;
    cfg.params.snapshot_interval = 25;
    cfg.params.record_positions = true;

    // Environment coupling: moderate — captures Au core stiffening
    // and softer organic shell response
    cfg.params.env_params.tau = 80.0;            // fs — faster than pure metal
    cfg.params.env_params.alpha = 0.6;           // Density-weighted
    cfg.params.env_params.beta = 0.4;            // Some orientational contribution
    cfg.params.env_params.gamma_steric = 0.25;   // Au core hardens under compression
    cfg.params.env_params.gamma_elec = -0.05;    // Mild screening at interface
    cfg.params.env_params.gamma_disp = 0.40;     // Dispersion enhancement for organic binding

    return cfg;
}

// ============================================================================
// Report Generator — Organic-Inorganic Study
// ============================================================================

std::string generate_organic_inorganic_report(
    const OrganicInorganicConfig& cfg,
    const SeedBeadStepper::SeedBeadResult& result,
    const SnapshotGraphCollector& collector)
{
    std::ostringstream md;
    md << std::fixed;

    md << "# Organic–Inorganic Synthesis Study\n\n";
    md << "**System:** Ethanol (C₂H₅OH) adsorption on Gold (Au) nanoparticle\n\n";
    md << "**Generated by VSEPR-SIM Seed-and-Bead Stepper**\n\n";
    md << "---\n\n";

    // System composition
    md << "## 1. System Composition\n\n";
    md << "| Species | Type ID | Count | σ (Å) | ε (kcal/mol) | Mass (amu) |\n";
    md << "|---------|---------|-------|-------|--------------|------------|\n";
    md << std::setprecision(3);
    md << "| Au (metal substrate) | 0 | " << cfg.n_au
       << " | " << cfg.system.bead_types[0].sigma
       << " | " << cfg.system.bead_types[0].epsilon
       << " | 196.967 |\n";
    md << "| EtOH (organic adsorbate) | 1 | " << cfg.n_ethanol
       << " | " << cfg.system.bead_types[1].sigma
       << " | " << cfg.system.bead_types[1].epsilon
       << " | 46.069 |\n";
    md << "| **Total** | | **" << (cfg.n_au + cfg.n_ethanol) << "** | | | |\n";
    md << "\n";

    // Cross-interaction
    double sigma_cross = 0.5 * (cfg.system.bead_types[0].sigma +
                                cfg.system.bead_types[1].sigma);
    double eps_cross = std::sqrt(cfg.system.bead_types[0].epsilon *
                                 cfg.system.bead_types[1].epsilon);
    md << "### 1.1 Cross-Interaction (Lorentz-Berthelot)\n\n";
    md << "| Parameter | Value |\n";
    md << "|-----------|-------|\n";
    md << std::setprecision(4);
    md << "| σ_Au–EtOH | " << sigma_cross << " Å |\n";
    md << "| ε_Au–EtOH | " << eps_cross << " kcal/mol |\n";
    md << "\n";

    // Outcome
    md << "## 2. Simulation Outcome\n\n";
    md << "| Metric | Value |\n";
    md << "|--------|-------|\n";
    md << "| Converged | " << (result.converged ? "**YES**" : "NO") << " |\n";
    md << std::setprecision(0);
    md << "| Steps taken | " << result.steps_taken << " |\n";
    md << std::setprecision(6);
    md << "| Final energy | " << result.final_energy << " kcal/mol |\n";
    md << "| Final RMS force | " << result.final_rms_force << " kcal/(mol·Å) |\n";
    md << "| Final mean η | " << result.final_avg_eta << " |\n";
    md << "\n";

    // Environment state
    md << "## 3. Environment-Responsive State (Final)\n\n";
    md << "| Observable | Value | Interpretation |\n";
    md << "|------------|-------|----------------|\n";
    md << std::setprecision(4);
    md << "| ⟨ρ⟩ | " << collector.avg_rho_series.final_val()
       << " | Density — higher for Au core |\n";
    md << "| ⟨C⟩ | " << collector.avg_C_series.final_val()
       << " | Coordination — Au FCC vs EtOH surface |\n";
    md << "| ⟨P₂⟩ | " << collector.avg_P2_series.final_val()
       << " | Orientational order |\n";
    md << "| ⟨η⟩ | " << collector.avg_eta_series.final_val()
       << " | Slow state — environment adaptation |\n";
    md << "| ⟨f⟩ | " << collector.avg_target_f_series.final_val()
       << " | Target function |\n";
    md << "\n";

    // Kernel modulation
    md << "## 4. Kernel Modulation (Final)\n\n";
    md << "| Channel | ⟨g⟩ | Physical Effect |\n";
    md << "|---------|-----|------------------|\n";
    md << "| Steric | " << collector.g_steric_series.final_val()
       << " | Au core hardening |\n";
    md << "| Electrostatic | " << collector.g_elec_series.final_val()
       << " | Interface charge screening |\n";
    md << "| Dispersion | " << collector.g_disp_series.final_val()
       << " | EtOH–Au binding enhancement |\n";
    md << "\n";

    // Scientific context
    md << "## 5. Scientific Context\n\n";
    md << "This simulation models the initial stages of self-assembled "
       << "monolayer (SAM) formation\n";
    md << "of ethanol-like organic molecules on a gold nanoparticle surface.\n\n";
    md << "Key physics captured:\n";
    md << "- Au core densification (high ρ → high η → steric hardening)\n";
    md << "- Organic shell formation (lower ρ → lower η → softer interactions)\n";
    md << "- Au–EtOH cross-interaction via Lorentz-Berthelot mixing\n";
    md << "- Environment-responsive kernel modulation at the interface\n\n";

    md << "---\n";
    md << "*Report generated by VSEPR-SIM v2.8.0 — "
       << "Deterministic atomistic platform*\n";

    return md.str();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== VSEPR-SIM: Organic–Inorganic Synthesis Study ===\n";
    std::cout << "Ethanol (C2H5OH) adsorption on Gold (Au) nanoparticle\n";
    std::cout << "Multi-species 6+9 Steady-State Step Function\n\n";

    // Parse bead counts
    uint32_t n_au = 27;
    uint32_t n_ethanol = 18;
    if (argc > 1) n_au = static_cast<uint32_t>(std::atoi(argv[1]));
    if (argc > 2) n_ethanol = static_cast<uint32_t>(std::atoi(argv[2]));

    // Build system
    auto cfg = build_organic_inorganic_system(n_au, n_ethanol);

    std::cout << "Species:\n";
    std::cout << "  Au beads:    " << cfg.n_au << "\n";
    std::cout << "  EtOH beads:  " << cfg.n_ethanol << "\n";
    std::cout << "  Total:       " << cfg.system.beads.size() << "\n\n";

    std::cout << "Bead types:\n";
    for (const auto& bt : cfg.system.bead_types) {
        std::cout << "  [" << bt.id << "] " << bt.name
                  << "  sigma=" << bt.sigma << " A"
                  << "  epsilon=" << bt.epsilon << " kcal/mol\n";
    }
    std::cout << "\n";

    double sigma_cross = 0.5 * (cfg.system.bead_types[0].sigma +
                                cfg.system.bead_types[1].sigma);
    double eps_cross = std::sqrt(cfg.system.bead_types[0].epsilon *
                                 cfg.system.bead_types[1].epsilon);
    std::cout << "Cross-interaction (Lorentz-Berthelot):\n";
    std::cout << "  sigma_Au-EtOH = " << sigma_cross << " A\n";
    std::cout << "  eps_Au-EtOH   = " << eps_cross << " kcal/mol\n\n";

    // Run simulation
    std::cout << "Running 6+9 stepper...\n";

    const size_t N = cfg.system.beads.size();
    std::vector<EnvironmentState> env_states(N);
    std::vector<atomistic::Vec3> velocities(N);
    std::vector<atomistic::Vec3> forces(N);
    FIREState fire;
    fire.dt = cfg.params.dt_initial;
    fire.alpha = cfg.params.fire_alpha_start;

    SeedBeadStepper::init(cfg.system, env_states, cfg.params);

    SnapshotGraphCollector collector;
    collector.snapshot_interval = cfg.params.snapshot_interval;

    SeedBeadStepper::SeedBeadResult result;

    for (uint64_t s = 0; s < cfg.params.max_steps; ++s) {
        auto record = SeedBeadStepper::step(
            cfg.system, env_states, velocities, forces, fire, cfg.params, s);
        collector.record(record);

        if (s % 500 == 0 || record.steady_state) {
            std::cout << "  Step " << s
                      << "  E=" << std::fixed << std::setprecision(4)
                      << record.total_energy
                      << "  F_rms=" << record.rms_force
                      << "  eta=" << record.avg_eta
                      << (record.steady_state ? "  ** STEADY STATE **" : "")
                      << "\n";
        }

        if (record.steady_state) {
            result.converged = true;
            result.steps_taken = s + 1;
            result.final_energy = record.total_energy;
            result.final_rms_force = record.rms_force;
            result.final_avg_eta = record.avg_eta;
            break;
        }
    }
    if (!result.converged) {
        result.steps_taken = cfg.params.max_steps;
        auto& last = collector.energy_series;
        result.final_energy = last.final_val();
        result.final_rms_force = collector.rms_force_series.final_val();
        result.final_avg_eta = collector.avg_eta_series.final_val();
    }

    std::cout << "\n=== Results ===\n";
    std::cout << "Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Steps: " << result.steps_taken << "\n";
    std::cout << "Final energy: " << result.final_energy << " kcal/mol\n";
    std::cout << "Final RMS force: " << result.final_rms_force << "\n";
    std::cout << "Final mean eta: " << result.final_avg_eta << "\n\n";

    // --- Export outputs ---
    std::string prefix = "organic_inorganic";

    // Markdown report
    {
        std::string report = generate_organic_inorganic_report(cfg, result, collector);
        std::string path = prefix + "_report.md";
        std::ofstream f(path);
        if (f.is_open()) { f << report; std::cout << "Written: " << path << "\n"; }
    }

    // CSV
    collector.export_timeseries_csv(prefix + "_timeseries.csv");
    std::cout << "Written: " << prefix + "_timeseries.csv\n";

    collector.export_snapshots_csv(prefix + "_snapshots.csv");
    std::cout << "Written: " << prefix + "_snapshots.csv\n";

    // Excel XML
    collector.export_excel(prefix + "_data.xml", "Organic-Inorganic Synthesis: EtOH on Au");
    std::cout << "Written: " << prefix + "_data.xml\n";

    // SolidWorks
    collector.export_solidworks_curve(prefix + "_structure.sldcrv");
    std::cout << "Written: " << prefix + "_structure.sldcrv\n";

    collector.export_solidworks_pointcloud(prefix + "_structure.xyz",
                                            "VSEPR-SIM EtOH-Au Synthesis");
    std::cout << "Written: " << prefix + "_structure.xyz\n";

    collector.export_solidworks_metadata_csv(prefix + "_points.csv");
    std::cout << "Written: " << prefix + "_points.csv\n";

    std::cout << "\nDone. All outputs generated.\n";
    return 0;
}
