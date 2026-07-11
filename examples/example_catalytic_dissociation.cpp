/**
 * example_catalytic_dissociation.cpp
 *
 * Example 2: Special Reaction — Catalytic H₂ Dissociation on Platinum
 * =====================================================================
 *
 * Models the dissociative chemisorption of molecular hydrogen (H₂) on a
 * platinum (Pt) catalyst surface — one of the most fundamental reactions
 * in heterogeneous catalysis.
 *
 * Three bead types (special reaction: 3-species dissociation):
 *   Type 0 — Pt (catalyst surface):  σ = 2.475 Å, ε = 7.20 kcal/mol
 *   Type 1 — H₂ (molecular hydrogen): σ = 2.93 Å, ε = 0.068 kcal/mol
 *   Type 2 — H  (dissociated atomic H): σ = 2.50 Å, ε = 0.034 kcal/mol
 *
 * Reaction modelled:
 *   H₂(g) → 2H(ads)  on Pt surface
 *
 * The environment-responsive layer captures the special reaction physics:
 *   - Pt surface beads: dense, high coordination → strong modulation
 *   - H₂ molecular beads: approach surface, experience increasing ρ
 *   - When η exceeds a dissociation threshold (η > η_diss), molecular H₂
 *     beads are replaced by two atomic H beads at adjacent Pt sites
 *   - Post-dissociation: H atoms relax into Pt surface hollow sites
 *
 * This is a "special reaction" because:
 *   1. The bead type changes mid-simulation (H₂ → 2H)
 *   2. The bead count changes (N increases by n_H₂ at dissociation)
 *   3. The reaction is triggered by environment state (η threshold)
 *   4. Three distinct species interact simultaneously
 *
 * Outputs:
 *   - Markdown report with reaction analysis
 *   - CSV timeseries and snapshot data
 *   - Excel XML spreadsheet
 *   - SolidWorks .sldcrv and .xyz point cloud
 *
 * Scientific context:
 *   H₂ dissociation on Pt is the rate-limiting step in:
 *   - Fuel cell anode reactions
 *   - Hydrogenation catalysis
 *   - Hydrogen storage/release cycles
 *   The barrier-free dissociative adsorption on Pt(111) is
 *   a textbook example of surface catalysis.
 *
 * Usage:
 *   catalytic-dissociation [n_pt] [n_h2]
 *   (defaults: 36 Pt beads, 12 H₂ beads → up to 24 H atoms)
 */

#include "coarse_grain/models/seed_bead_stepper.hpp"
#include "coarse_grain/report/snapshot_graph.hpp"
#include "coarse_grain/report/excel_export.hpp"
#include "coarse_grain/report/solidworks_export.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace coarse_grain;

// ============================================================================
// Dissociation Event Tracking
// ============================================================================

struct DissociationEvent {
    uint64_t step{};
    uint32_t h2_bead_id{};
    double eta_at_trigger{};
    double rho_at_trigger{};
    atomistic::Vec3 position{};
};

// ============================================================================
// System Builder — 3-Species Catalytic System
// ============================================================================

struct CatalyticConfig {
    BeadSystem system;
    SeedBeadParams params;
    uint32_t n_pt{};
    uint32_t n_h2{};
    double eta_dissociation{0.45};   // η threshold for H₂ → 2H
    std::vector<bool> is_molecular;  // Track which H beads are still H₂
    std::vector<DissociationEvent> events;
};

/**
 * Build a Pt surface slab with H₂ molecules above it.
 *
 * Pt slab: 6×6×1 = 36 beads in a (111)-like surface arrangement
 * H₂ molecules: placed 3–5 Å above the Pt surface
 */
CatalyticConfig build_catalytic_system(
    uint32_t n_pt = 36,
    uint32_t n_h2 = 12)
{
    CatalyticConfig cfg;
    cfg.n_pt = n_pt;
    cfg.n_h2 = n_h2;

    // --- Bead types ---

    // Type 0: Platinum (Pt) — catalyst surface
    // σ = a₀/2^(1/6) ≈ 3.924/1.122 ≈ 3.497, scaled for CG
    // ε from cohesive energy: 5.84 eV / 6 ≈ 0.973 eV ≈ 22.44 kcal/mol, scaled
    BeadType pt_type;
    pt_type.name = "Pt";
    pt_type.id = 0;
    pt_type.sigma = 2.475;      // Pt–Pt effective σ (Å)
    pt_type.epsilon = 7.20;     // Pt–Pt effective ε (kcal/mol)
    cfg.system.bead_types.push_back(pt_type);

    // Type 1: Molecular hydrogen (H₂) — intact molecule
    // σ from Silvera-Goldman: ~2.93 Å
    // ε from well depth: ~3.0 meV ≈ 0.068 kcal/mol
    BeadType h2_type;
    h2_type.name = "H2";
    h2_type.id = 1;
    h2_type.sigma = 2.93;      // H₂–H₂ σ (Å)
    h2_type.epsilon = 0.068;   // H₂–H₂ ε (kcal/mol)
    cfg.system.bead_types.push_back(h2_type);

    // Type 2: Atomic hydrogen (H) — dissociated product
    // σ smaller than H₂ (single atom)
    // ε also smaller — binding dominated by Pt–H interaction
    BeadType h_type;
    h_type.name = "H";
    h_type.id = 2;
    h_type.sigma = 2.50;       // H–H σ (Å)
    h_type.epsilon = 0.034;    // H–H ε (kcal/mol)
    cfg.system.bead_types.push_back(h_type);

    // --- Pt surface slab ---
    // Arrange in a hexagonal (111)-like pattern
    double a_pt = 3.924;       // Pt lattice constant (Å)
    double d_nn = a_pt / std::sqrt(2.0);  // Nearest-neighbour distance ~2.775 Å

    int nx = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n_pt))));
    int ny = nx;
    uint32_t count = 0;

    for (int ix = 0; ix < nx && count < n_pt; ++ix) {
        for (int iy = 0; iy < ny && count < n_pt; ++iy) {
            Bead b;
            double x_shift = (iy % 2 == 1) ? 0.5 * d_nn : 0.0;
            b.position = {
                ix * d_nn + x_shift,
                iy * d_nn * std::sqrt(3.0) / 2.0,
                0.0    // Surface plane at z = 0
            };
            b.mass = 195.084;   // Pt atomic mass (amu)
            b.charge = 0.0;
            b.type_id = 0;      // Pt
            cfg.system.beads.push_back(b);
            ++count;
        }
    }

    // Centre the slab at origin
    double cx = 0.0, cy = 0.0;
    for (uint32_t i = 0; i < n_pt; ++i) {
        cx += cfg.system.beads[i].position.x;
        cy += cfg.system.beads[i].position.y;
    }
    cx /= n_pt;
    cy /= n_pt;
    for (uint32_t i = 0; i < n_pt; ++i) {
        cfg.system.beads[i].position.x -= cx;
        cfg.system.beads[i].position.y -= cy;
    }

    // --- H₂ molecules above the surface ---
    // Place in a grid 3.5–5 Å above the Pt plane
    double z_approach = 4.0;    // Starting height above surface (Å)
    int nh_side = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n_h2))));
    double h_spacing = d_nn * 1.5;
    double h_offset = -0.5 * h_spacing * (nh_side - 1);
    count = 0;

    for (int ix = 0; ix < nh_side && count < n_h2; ++ix) {
        for (int iy = 0; iy < nh_side && count < n_h2; ++iy) {
            Bead b;
            b.position = {
                h_offset + ix * h_spacing,
                h_offset + iy * h_spacing,
                z_approach + 0.5 * (count % 3)  // Slight height variation
            };
            b.mass = 2.016;     // H₂ molecular mass (amu)
            b.charge = 0.0;
            b.type_id = 1;      // H₂ (molecular)
            cfg.system.beads.push_back(b);
            cfg.is_molecular.push_back(true);
            ++count;
        }
    }

    cfg.system.source_atom_count = n_pt + n_h2;

    // --- Parameters tuned for catalytic surface reaction ---
    cfg.params.dt_initial = 0.5;        // Smaller timestep (light H₂)
    cfg.params.dt_max = 5.0;
    cfg.params.f_tol = 0.03;
    cfg.params.e_tol = 1.0e-6;
    cfg.params.max_steps = 4000;
    cfg.params.snapshot_interval = 20;
    cfg.params.record_positions = true;

    // Environment coupling: strong — catalytic surface is highly responsive
    cfg.params.env_params.tau = 40.0;             // Fast response (catalytic)
    cfg.params.env_params.alpha = 0.7;            // Density-dominated
    cfg.params.env_params.beta = 0.3;
    cfg.params.env_params.gamma_steric = 0.35;    // Strong Pt stiffening
    cfg.params.env_params.gamma_elec = -0.10;     // Moderate charge screening
    cfg.params.env_params.gamma_disp = 0.60;      // Strong dispersion (chemisorption)
    cfg.params.env_params.r_cutoff = 6.0;         // Shorter cutoff for surface

    cfg.eta_dissociation = 0.45;   // Dissociation trigger

    return cfg;
}

// ============================================================================
// Dissociation Logic — Special Reaction Step
// ============================================================================

/**
 * Check for and execute H₂ dissociation events.
 *
 * When an H₂ bead's η exceeds the dissociation threshold
 * (indicating strong surface interaction), it is replaced by
 * two atomic H beads placed at adjacent positions.
 *
 * This is the "special" part of this reaction example:
 * the bead identity changes based on environment state.
 */
void check_dissociation(
    CatalyticConfig& cfg,
    std::vector<EnvironmentState>& env_states,
    std::vector<atomistic::Vec3>& velocities,
    std::vector<atomistic::Vec3>& forces,
    uint64_t step_index)
{
    const uint32_t n_pt = cfg.n_pt;
    size_t N = cfg.system.beads.size();

    for (size_t i = n_pt; i < N; ++i) {
        size_t mol_idx = i - n_pt;
        if (mol_idx >= cfg.is_molecular.size()) continue;
        if (!cfg.is_molecular[mol_idx]) continue;          // Already dissociated
        if (cfg.system.beads[i].type_id != 1) continue;    // Not H₂

        double eta_i = (i < env_states.size()) ? env_states[i].eta : 0.0;
        if (eta_i < cfg.eta_dissociation) continue;

        // --- Dissociation event! ---
        DissociationEvent evt;
        evt.step = step_index;
        evt.h2_bead_id = static_cast<uint32_t>(i);
        evt.eta_at_trigger = eta_i;
        evt.rho_at_trigger = env_states[i].rho;
        evt.position = cfg.system.beads[i].position;
        cfg.events.push_back(evt);

        // Convert this H₂ bead → first H atom (in-place)
        cfg.system.beads[i].type_id = 2;        // H (atomic)
        cfg.system.beads[i].mass = 1.008;        // Single H atom
        cfg.system.beads[i].position.x -= 0.37;  // Displace ~0.74 Å apart (H–H bond)
        cfg.system.beads[i].position.z -= 0.5;    // Closer to surface

        // Create second H atom
        Bead h2;
        h2.position = evt.position;
        h2.position.x += 0.37;
        h2.position.z -= 0.5;
        h2.mass = 1.008;
        h2.charge = 0.0;
        h2.type_id = 2;  // H (atomic)
        cfg.system.beads.push_back(h2);

        // Extend state vectors
        env_states.push_back(env_states[i]);  // Copy parent state
        velocities.push_back({0.0, 0.0, 0.0});
        forces.push_back({0.0, 0.0, 0.0});

        cfg.is_molecular[mol_idx] = false;
    }
}

// ============================================================================
// Report Generator — Catalytic Reaction Study
// ============================================================================

std::string generate_catalytic_report(
    const CatalyticConfig& cfg,
    const SeedBeadStepper::SeedBeadResult& result,
    const SnapshotGraphCollector& collector)
{
    std::ostringstream md;
    md << std::fixed;

    md << "# Special Reaction Study: H₂ Dissociation on Platinum\n\n";
    md << "**System:** Molecular hydrogen (H₂) dissociative chemisorption "
       << "on Pt(111) surface\n\n";
    md << "**Reaction:** H₂(g) → 2H(ads) on Pt catalyst\n\n";
    md << "**Generated by VSEPR-SIM Seed-and-Bead Stepper**\n\n";
    md << "---\n\n";

    // System composition
    md << "## 1. System Composition\n\n";
    md << "### 1.1 Initial State\n\n";
    md << "| Species | Type ID | Count | σ (Å) | ε (kcal/mol) | Mass (amu) |\n";
    md << "|---------|---------|-------|-------|--------------|------------|\n";
    md << std::setprecision(3);
    md << "| Pt (catalyst) | 0 | " << cfg.n_pt
       << " | " << cfg.system.bead_types[0].sigma
       << " | " << cfg.system.bead_types[0].epsilon
       << " | 195.084 |\n";
    md << "| H₂ (molecular) | 1 | " << cfg.n_h2
       << " | " << cfg.system.bead_types[1].sigma
       << " | " << cfg.system.bead_types[1].epsilon
       << " | 2.016 |\n";
    md << "| H (atomic) | 2 | 0 (produced by reaction)"
       << " | " << cfg.system.bead_types[2].sigma
       << " | " << cfg.system.bead_types[2].epsilon
       << " | 1.008 |\n";
    md << "\n";

    // Cross-interactions
    md << "### 1.2 Cross-Interactions (Lorentz-Berthelot)\n\n";
    md << "| Pair | σ (Å) | ε (kcal/mol) |\n";
    md << "|------|-------|---------------|\n";
    md << std::setprecision(4);
    for (int a = 0; a < 3; ++a) {
        for (int b = a; b < 3; ++b) {
            double s = 0.5 * (cfg.system.bead_types[a].sigma +
                              cfg.system.bead_types[b].sigma);
            double e = std::sqrt(cfg.system.bead_types[a].epsilon *
                                 cfg.system.bead_types[b].epsilon);
            md << "| " << cfg.system.bead_types[a].name << "–"
               << cfg.system.bead_types[b].name
               << " | " << s << " | " << e << " |\n";
        }
    }
    md << "\n";

    // Dissociation events
    md << "## 2. Dissociation Events\n\n";
    md << "**Trigger:** η > " << std::setprecision(2) << cfg.eta_dissociation
       << " (environment-responsive threshold)\n\n";

    if (cfg.events.empty()) {
        md << "*No dissociation events occurred within the simulation window.*\n\n";
    } else {
        md << "| # | Step | Bead ID | η at trigger | ρ at trigger | Position (Å) |\n";
        md << "|---|------|---------|-------------|-------------|---------------|\n";
        md << std::setprecision(4);
        for (size_t i = 0; i < cfg.events.size(); ++i) {
            auto& e = cfg.events[i];
            md << "| " << (i + 1)
               << " | " << e.step
               << " | " << e.h2_bead_id
               << " | " << e.eta_at_trigger
               << " | " << e.rho_at_trigger
               << " | (" << std::setprecision(2)
               << e.position.x << ", " << e.position.y << ", " << e.position.z
               << ") |\n";
        }
        md << std::setprecision(4);
        md << "\n";
        md << "**Total dissociations:** " << cfg.events.size()
           << " / " << cfg.n_h2 << " H₂ molecules\n";
        md << "**Conversion:** "
           << std::setprecision(1)
           << (100.0 * cfg.events.size() / std::max(cfg.n_h2, 1u))
           << "%\n\n";
    }

    // Simulation outcome
    md << "## 3. Simulation Outcome\n\n";
    md << "| Metric | Value |\n";
    md << "|--------|-------|\n";
    md << "| Converged | " << (result.converged ? "**YES**" : "NO") << " |\n";
    md << std::setprecision(0);
    md << "| Steps taken | " << result.steps_taken << " |\n";
    md << std::setprecision(6);
    md << "| Final energy | " << result.final_energy << " kcal/mol |\n";
    md << "| Final RMS force | " << result.final_rms_force << " kcal/(mol·Å) |\n";
    md << "| Final mean η | " << result.final_avg_eta << " |\n";
    md << "| Final bead count | " << cfg.system.beads.size() << " |\n";
    md << "\n";

    // Environment state
    md << "## 4. Environment-Responsive State (Final)\n\n";
    md << "| Observable | Value |\n";
    md << "|------------|-------|\n";
    md << std::setprecision(4);
    md << "| ⟨ρ⟩ | " << collector.avg_rho_series.final_val() << " |\n";
    md << "| ⟨C⟩ | " << collector.avg_C_series.final_val() << " |\n";
    md << "| ⟨P₂⟩ | " << collector.avg_P2_series.final_val() << " |\n";
    md << "| ⟨η⟩ | " << collector.avg_eta_series.final_val() << " |\n";
    md << "| ⟨f⟩ | " << collector.avg_target_f_series.final_val() << " |\n";
    md << "\n";

    // Kernel modulation
    md << "## 5. Kernel Modulation (Final)\n\n";
    md << "| Channel | ⟨g⟩ | Physical Effect |\n";
    md << "|---------|-----|------------------|\n";
    md << "| Steric | " << collector.g_steric_series.final_val()
       << " | Pt surface hardening |\n";
    md << "| Electrostatic | " << collector.g_elec_series.final_val()
       << " | Charge redistribution at Pt–H interface |\n";
    md << "| Dispersion | " << collector.g_disp_series.final_val()
       << " | Chemisorption binding enhancement |\n";
    md << "\n";

    // Scientific context
    md << "## 6. Scientific Context\n\n";
    md << "H₂ dissociative adsorption on Pt(111) is the prototypical "
       << "example of catalytic\n";
    md << "bond-breaking driven by surface–adsorbate interaction.\n\n";
    md << "Key physics captured in this simulation:\n";
    md << "- **Environment-triggered dissociation:** H₂ → 2H when η > η_diss\n";
    md << "- **Bead identity change:** type_id transitions from H₂ (1) to H (2)\n";
    md << "- **Bead count change:** N increases as each H₂ produces 2 H atoms\n";
    md << "- **3-species Lorentz-Berthelot mixing:** Pt–Pt, Pt–H₂, Pt–H, "
       << "H₂–H₂, H₂–H, H–H\n";
    md << "- **Catalytic kernel modulation:** strong γ_disp enhances Pt–H binding\n\n";
    md << "Applications:\n";
    md << "- Fuel cell anode half-reaction: H₂ → 2H⁺ + 2e⁻\n";
    md << "- Catalytic hydrogenation precursor step\n";
    md << "- Hydrogen storage / release dynamics\n\n";

    md << "---\n";
    md << "*Report generated by VSEPR-SIM v2.8.0 — "
       << "Deterministic atomistic platform*\n";

    return md.str();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== VSEPR-SIM: Special Reaction — Catalytic H2 Dissociation ===\n";
    std::cout << "H2(g) -> 2H(ads) on Pt(111) surface\n";
    std::cout << "3-species 6+9 Steady-State Step Function with Reaction Events\n\n";

    // Parse bead counts
    uint32_t n_pt = 36;
    uint32_t n_h2 = 12;
    if (argc > 1) n_pt = static_cast<uint32_t>(std::atoi(argv[1]));
    if (argc > 2) n_h2 = static_cast<uint32_t>(std::atoi(argv[2]));

    // Build system
    auto cfg = build_catalytic_system(n_pt, n_h2);

    std::cout << "Initial species:\n";
    std::cout << "  Pt beads:  " << cfg.n_pt << "\n";
    std::cout << "  H2 beads:  " << cfg.n_h2 << "\n";
    std::cout << "  Total:     " << cfg.system.beads.size() << "\n\n";

    std::cout << "Bead types:\n";
    for (const auto& bt : cfg.system.bead_types) {
        std::cout << "  [" << bt.id << "] " << bt.name
                  << "  sigma=" << bt.sigma << " A"
                  << "  epsilon=" << bt.epsilon << " kcal/mol\n";
    }
    std::cout << "\nDissociation threshold: eta > "
              << cfg.eta_dissociation << "\n\n";

    // --- Run simulation with reaction logic ---
    std::cout << "Running 6+9 stepper with dissociation checks...\n";

    size_t N = cfg.system.beads.size();
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
    size_t prev_events = 0;

    for (uint64_t s = 0; s < cfg.params.max_steps; ++s) {
        // Resize state vectors if beads were added by dissociation
        N = cfg.system.beads.size();
        if (velocities.size() < N) {
            velocities.resize(N, {0.0, 0.0, 0.0});
            forces.resize(N, {0.0, 0.0, 0.0});
            env_states.resize(N);
        }

        auto record = SeedBeadStepper::step(
            cfg.system, env_states, velocities, forces, fire, cfg.params, s);
        collector.record(record);

        // Check for dissociation after each step
        check_dissociation(cfg, env_states, velocities, forces, s);

        // Report dissociation events
        if (cfg.events.size() > prev_events) {
            for (size_t e = prev_events; e < cfg.events.size(); ++e) {
                auto& evt = cfg.events[e];
                std::cout << "  ** DISSOCIATION at step " << evt.step
                          << "  bead=" << evt.h2_bead_id
                          << "  eta=" << std::fixed << std::setprecision(4)
                          << evt.eta_at_trigger
                          << "  N_beads=" << cfg.system.beads.size() << "\n";
            }
            prev_events = cfg.events.size();
        }

        if (s % 500 == 0 || record.steady_state) {
            std::cout << "  Step " << s
                      << "  E=" << std::fixed << std::setprecision(4)
                      << record.total_energy
                      << "  F_rms=" << record.rms_force
                      << "  eta=" << record.avg_eta
                      << "  N=" << cfg.system.beads.size()
                      << "  dissoc=" << cfg.events.size() << "/" << cfg.n_h2
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
        result.final_energy = collector.energy_series.final_val();
        result.final_rms_force = collector.rms_force_series.final_val();
        result.final_avg_eta = collector.avg_eta_series.final_val();
    }

    std::cout << "\n=== Results ===\n";
    std::cout << "Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Steps: " << result.steps_taken << "\n";
    std::cout << "Final energy: " << result.final_energy << " kcal/mol\n";
    std::cout << "Final RMS force: " << result.final_rms_force << "\n";
    std::cout << "Final mean eta: " << result.final_avg_eta << "\n";
    std::cout << "Final bead count: " << cfg.system.beads.size() << "\n";
    std::cout << "Dissociation events: " << cfg.events.size()
              << " / " << cfg.n_h2 << " H2 molecules\n";
    std::cout << "Conversion: "
              << std::setprecision(1)
              << (100.0 * cfg.events.size() / std::max(cfg.n_h2, 1u))
              << "%\n\n";

    // --- Export outputs ---
    std::string prefix = "catalytic_dissociation";

    // Markdown report
    {
        std::string report = generate_catalytic_report(cfg, result, collector);
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
    collector.export_excel(prefix + "_data.xml",
                            "Special Reaction: H2 Dissociation on Pt");
    std::cout << "Written: " << prefix + "_data.xml\n";

    // SolidWorks
    collector.export_solidworks_curve(prefix + "_structure.sldcrv");
    std::cout << "Written: " << prefix + "_structure.sldcrv\n";

    collector.export_solidworks_pointcloud(prefix + "_structure.xyz",
                                            "VSEPR-SIM H2-Pt Catalysis");
    std::cout << "Written: " << prefix + "_structure.xyz\n";

    collector.export_solidworks_metadata_csv(prefix + "_points.csv");
    std::cout << "Written: " << prefix + "_points.csv\n";

    std::cout << "\nDone. All outputs generated.\n";
    return 0;
}
