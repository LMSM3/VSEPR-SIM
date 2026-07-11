/**
 * example_universal_reaction_engine.cpp
 *
 * Universal Automated Chemical Reaction Engine — Full Demonstration
 * ==================================================================
 *
 * Runs three chemically distinct reactions through the thermal-atomistic
 * simulation pipeline, generating comprehensive reports and SolidWorks-
 * compatible material data for each.
 *
 * Reaction 1 — Organic Synthesis with Inorganic Reagent:
 *   C₆H₆ + HNO₃ →[H₂SO₄] C₆H₅NO₂ + H₂O
 *   Nitration of benzene via electrophilic aromatic substitution.
 *
 * Reaction 2 — Special Inorganic Precipitation:
 *   Th(NO₃)₄ + 2 H₂C₂O₄ + 6 H₂O → Th(C₂O₄)₂·6H₂O↓ + 4 HNO₃
 *   Thorium oxalate sludge formation.
 *
 * Reaction 3 — Thermal Decomposition (salt → gas + powder):
 *   2 Cu(NO₃)₂ → 2 CuO(s) + 4 NO₂(g) + O₂(g)
 *   Copper nitrate decomposition.
 *
 * Outputs per reaction (12–22 elements):
 *   - Markdown report with full analysis
 *   - CSV timeseries (energy, forces, η, etc.)
 *   - SolidWorks CFD material data CSV (22 fields per species)
 *   - SolidWorks material XML (.sldmat format)
 *   - Excel XML spreadsheet
 *   - SolidWorks .sldcrv / .xyz point cloud
 *
 * Usage:
 *   universal-reaction-engine [reaction_index]
 *     0 = all reactions (default)
 *     1 = benzene nitration only
 *     2 = thorium oxalate only
 *     3 = copper nitrate decomposition only
 */

#include "coarse_grain/chemistry/reaction_engine.hpp"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace coarse_grain;
using namespace coarse_grain::chemistry;

// ============================================================================
// Run a single reaction through the full pipeline
// ============================================================================

void run_reaction_pipeline(const ChemicalReaction& rxn, const std::string& prefix) {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << " " << rxn.name << "\n";
    std::cout << "============================================================\n";
    std::cout << "Type:      " << rxn.reaction_type << "\n";
    std::cout << "Equation:  " << rxn.equation << "\n";
    std::cout << "Conditions:" << rxn.conditions << "\n\n";

    // --- Display species ---
    std::cout << "Species:\n";
    for (auto& e : rxn.entries) {
        std::cout << "  [" << role_label(e.role) << "] "
                  << e.coefficient << " x " << e.species.name
                  << " (" << e.species.formula << ") "
                  << phase_label(e.species.phase)
                  << "  MW=" << e.species.molecular_weight
                  << "  dHf=" << e.species.delta_Hf << " kcal/mol\n";
    }
    std::cout << "\n";

    // --- Thermodynamics ---
    std::cout << "Thermodynamics:\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  dH_rxn = " << rxn.delta_H_rxn << " kcal/mol"
              << (rxn.delta_H_rxn < 0 ? " (exothermic)" : " (endothermic)") << "\n";
    std::cout << "  dG_rxn = " << rxn.delta_G_rxn << " kcal/mol"
              << (rxn.delta_G_rxn < 0 ? " (spontaneous)" : " (non-spontaneous at 298K)") << "\n";
    std::cout << std::setprecision(3);
    std::cout << "  dS_rxn = " << rxn.delta_S_rxn << " cal/(mol*K)\n";
    std::cout << "  Ea     = " << rxn.activation_energy << " kcal/mol\n";
    std::cout << "  K_eq   = " << std::scientific << rxn.equilibrium_constant
              << std::fixed << "\n\n";

    // --- Mass balance ---
    auto mb = rxn.verify_mass_balance();
    std::cout << "Mass balance: " << (mb.balanced ? "BALANCED" : "IMBALANCED") << "\n";
    for (auto& [elem, count] : mb.reactant_elements) {
        double p = 0.0;
        auto it = mb.product_elements.find(elem);
        if (it != mb.product_elements.end()) p = it->second;
        std::cout << "  " << elem << ": reactant=" << std::setprecision(1)
                  << count << " product=" << p << "\n";
    }
    std::cout << "\n";

    // --- Run simulation ---
    std::cout << "Running 6+9 thermal-atomistic simulation...\n";
    auto result = ReactionEngine::run_reaction(rxn);

    std::cout << "  Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "  Steps:     " << result.steps_taken << "\n";
    std::cout << std::setprecision(6);
    std::cout << "  Final E:   " << result.final_energy << " kcal/mol\n";
    std::cout << "  Final F:   " << result.final_rms_force << "\n";
    std::cout << "  Final eta: " << result.final_avg_eta << "\n\n";

    // --- Bonding predictions ---
    std::cout << "Bonding predictions:\n";
    for (auto& bp : result.bond_predictions) {
        std::cout << "  " << bp.atom_pair << ": "
                  << bond_type_label(bp.predicted_type)
                  << " (" << std::setprecision(1) << bp.predicted_strength
                  << " kcal/mol) -- " << bp.rationale << "\n";
    }
    std::cout << "\n";

    // --- By-products ---
    auto bps = rxn.byproducts();
    if (!bps.empty()) {
        std::cout << "By-products:\n";
        for (auto& e : bps) {
            std::cout << "  " << e.coefficient << " x " << e.species.name
                      << " (" << e.species.formula << ") "
                      << phase_label(e.species.phase) << "\n";
        }
        std::cout << "\n";
    }

    // --- Export outputs ---
    std::cout << "Exporting outputs...\n";

    // 1. Markdown report
    {
        std::string report = ReactionEngine::generate_reaction_report(rxn, result);
        std::string path = prefix + "_report.md";
        std::ofstream f(path);
        if (f.is_open()) { f << report; std::cout << "  Written: " << path << "\n"; }
    }

    // 2. SolidWorks CFD material data (22-element CSV)
    {
        std::string path = prefix + "_cfd_materials.csv";
        if (ReactionEngine::export_cfd_material_csv(path, result.material_records, rxn.name))
            std::cout << "  Written: " << path << " (" << result.material_records.size()
                      << " species, 22 fields each)\n";
    }

    // 3. SolidWorks material XML
    {
        std::string path = prefix + "_materials.sldmat";
        if (ReactionEngine::export_solidworks_material_xml(path, result.material_records, rxn.name))
            std::cout << "  Written: " << path << "\n";
    }

    // 4. Map to bead system and run snapshot collector for remaining exports
    auto system = ReactionEngine::map_reaction_to_beads(rxn);
    auto params = ReactionEngine::build_reaction_params(rxn);
    size_t N = system.beads.size();
    std::vector<EnvironmentState> env_states(N);
    std::vector<atomistic::Vec3> velocities(N);
    std::vector<atomistic::Vec3> forces(N);
    FIREState fire;
    fire.dt = params.dt_initial;
    fire.alpha = params.fire_alpha_start;
    SeedBeadStepper::init(system, env_states, params);

    SnapshotGraphCollector collector;
    collector.snapshot_interval = params.snapshot_interval;

    for (uint64_t s = 0; s < params.max_steps; ++s) {
        auto rec = SeedBeadStepper::step(
            system, env_states, velocities, forces, fire, params, s);
        collector.record(rec);
        if (rec.steady_state) break;
    }

    // 5. CSV timeseries
    collector.export_timeseries_csv(prefix + "_timeseries.csv");
    std::cout << "  Written: " << prefix + "_timeseries.csv\n";

    collector.export_snapshots_csv(prefix + "_snapshots.csv");
    std::cout << "  Written: " << prefix + "_snapshots.csv\n";

    // 6. Excel XML
    collector.export_excel(prefix + "_data.xml", rxn.name);
    std::cout << "  Written: " << prefix + "_data.xml\n";

    // 7. SolidWorks structural
    collector.export_solidworks_curve(prefix + "_structure.sldcrv");
    std::cout << "  Written: " << prefix + "_structure.sldcrv\n";

    collector.export_solidworks_pointcloud(prefix + "_structure.xyz",
                                           "VSEPR-SIM " + rxn.name);
    std::cout << "  Written: " << prefix + "_structure.xyz\n";

    collector.export_solidworks_metadata_csv(prefix + "_points.csv");
    std::cout << "  Written: " << prefix + "_points.csv\n";

    std::cout << "\nDone: " << rxn.name << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "================================================================\n";
    std::cout << " VSEPR-SIM: Universal Automated Chemical Reaction Engine\n";
    std::cout << " Thermal-Atomistic + Macro Structures + Bonding Predictions\n";
    std::cout << " SolidWorks Output (CFD + Material Data, 12-22 Elements)\n";
    std::cout << "================================================================\n";

    int rxn_select = 0;
    if (argc > 1) rxn_select = std::atoi(argv[1]);

    auto reactions = library::all_library_reactions();

    if (rxn_select == 0) {
        std::cout << "\nRunning ALL " << reactions.size() << " reactions...\n";
        std::vector<std::string> prefixes = {
            "rxn1_benzene_nitration",
            "rxn2_thorium_oxalate",
            "rxn3_copper_decomposition"
        };
        for (size_t i = 0; i < reactions.size(); ++i) {
            run_reaction_pipeline(reactions[i], prefixes[i]);
        }
    }
    else if (rxn_select >= 1 && rxn_select <= static_cast<int>(reactions.size())) {
        std::vector<std::string> prefixes = {
            "rxn1_benzene_nitration",
            "rxn2_thorium_oxalate",
            "rxn3_copper_decomposition"
        };
        run_reaction_pipeline(reactions[rxn_select - 1], prefixes[rxn_select - 1]);
    }
    else {
        std::cerr << "Invalid reaction index. Use 0 (all), 1, 2, or 3.\n";
        return 1;
    }

    std::cout << "\n================================================================\n";
    std::cout << " All outputs generated. Files ready for:\n";
    std::cout << "   - SolidWorks Flow Simulation (CFD material CSV)\n";
    std::cout << "   - SolidWorks Material Library (.sldmat XML)\n";
    std::cout << "   - SolidWorks Curve Import (.sldcrv)\n";
    std::cout << "   - Excel / LibreOffice (XML spreadsheet)\n";
    std::cout << "   - Python visualization (CSV timeseries)\n";
    std::cout << "================================================================\n";

    return 0;
}
