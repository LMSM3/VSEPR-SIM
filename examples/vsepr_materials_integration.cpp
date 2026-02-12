/**
 * VSEPR-Sim → Materials Subsystem Integration
 * Demonstrates how molecular simulation connects to materials analysis
 * "Weaving back" to original VSEPR code
 */

#include "subsystem/metallic_sim.hpp"
#include "molecular/unified_types.hpp"
#include "gui/data_pipe.hpp"
#include "sim/molecule.hpp"
#include "core/types.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>

using namespace vsepr;
using namespace vsepr::subsystem;
using namespace vsepr::gui;
using namespace vsepr::molecular;

// Use unified type instead of duplicate definition
using MolecularMaterialProperties = vsepr::molecular::MolecularMaterialProperties;

// ============================================================================
// VSEPR → Materials Bridge
// ============================================================================

class VSEPRMaterialsBridge {
private:
    MetallicSimulator materials_sim_;
    
    // Data pipes connecting VSEPR to materials analysis
    std::shared_ptr<DataPipe<Molecule>> molecule_pipe_;
    std::shared_ptr<DataPipe<MolecularMaterialProperties>> analysis_pipe_;
    
public:
    VSEPRMaterialsBridge() {
        molecule_pipe_ = std::make_shared<DataPipe<Molecule>>("vsepr_molecules");
        analysis_pipe_ = std::make_shared<DataPipe<MolecularMaterialProperties>>("materials_analysis");
        
        // Subscribe to molecule updates
        molecule_pipe_->subscribe([this](const Molecule& mol) {
            this->process_molecule(mol);
        });
    }
    
    void process_molecule(const Molecule& mol) {
        std::cout << "\n[VSEPR→Materials Bridge] Processing molecule with " 
                  << mol.num_atoms() << " atoms\n";
        
        // Analyze molecular structure and recommend materials
        analyze_and_recommend(mol);
    }
    
    void analyze_and_recommend(const Molecule& mol) {
        // For demo: If molecule contains Ni/Cr/Mo, recommend Hastelloy alloys
        bool has_ni = false, has_cr = false, has_mo = false;
        
        for (const auto& atom : mol.atoms) {
            if (atom.Z == 28) has_ni = true;  // Nickel
            if (atom.Z == 24) has_cr = true;  // Chromium
            if (atom.Z == 42) has_mo = true;  // Molybdenum
        }
        
        if (has_ni || has_cr || has_mo) {
            std::cout << "  → Detected transition metals (Ni/Cr/Mo)\n";
            std::cout << "  → Recommending Hastelloy alloys for container material\n\n";
            
            // Search for suitable materials
            auto candidates = materials_sim_.search_materials(
                650.0, 850.0,  // tensile range
                250.0, 450.0   // yield range
            );
            
            std::cout << "  Recommended materials (" << candidates.size() << " candidates):\n";
            for (const auto& mat : candidates) {
                std::cout << "    - " << std::setw(20) << std::left << mat.material
                          << " | σ_b: " << mat.tensile_strength_MPa << " MPa"
                          << " | Safety for corrosive environment\n";
            }
        }
    }
    
    std::shared_ptr<DataPipe<Molecule>> get_molecule_pipe() { return molecule_pipe_; }
    std::shared_ptr<DataPipe<MolecularMaterialProperties>> get_analysis_pipe() { return analysis_pipe_; }
};

// ============================================================================
// Demo 1: Organometallic Complex → Materials Selection
// ============================================================================

void demo_organometallic_catalyst() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Demo 1: Organometallic Catalyst → Reactor Materials          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    // Create Ni-based catalyst molecule (simplified)
    Molecule catalyst;
    catalyst.add_atom(28, 0.0, 0.0, 0.0);  // Ni center
    catalyst.add_atom(6, 1.8, 0.0, 0.0);   // C ligand
    catalyst.add_atom(6, -1.8, 0.0, 0.0);  // C ligand
    catalyst.add_atom(7, 0.0, 1.8, 0.0);   // N ligand
    catalyst.add_atom(7, 0.0, -1.8, 0.0);  // N ligand
    
    std::cout << "\n[VSEPR Simulation] Created Ni-based catalyst complex\n";
    std::cout << "  Atoms: " << catalyst.num_atoms() << "\n";
    std::cout << "  Central metal: Ni (Z=28)\n";
    std::cout << "  Ligands: C, N (tetrahedral coordination)\n";
    
    // Create bridge and process
    VSEPRMaterialsBridge bridge;
    bridge.get_molecule_pipe()->push(catalyst);
}

// ============================================================================
// Demo 2: Stress Testing Workflow (VSEPR → Materials → Failure)
// ============================================================================

void demo_stress_testing_workflow() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Demo 2: Complete Workflow (VSEPR → Materials → Failure)      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    // Step 1: VSEPR molecular simulation
    Molecule corrosive_molecule;
    corrosive_molecule.add_atom(24, 0.0, 0.0, 0.0);  // Cr
    corrosive_molecule.add_atom(8, 1.6, 0.0, 0.0);   // O
    corrosive_molecule.add_atom(8, -1.6, 0.0, 0.0);  // O
    corrosive_molecule.add_atom(8, 0.0, 1.6, 0.0);   // O
    
    std::cout << "\n[Step 1: VSEPR] Simulated CrO₃ (corrosive oxidizer)\n";
    std::cout << "  → High oxidation potential\n";
    std::cout << "  → Requires corrosion-resistant container\n";
    
    // Step 2: Materials selection
    MetallicSimulator sim;
    auto material = sim.get_material("Hastelloy C-276");
    
    std::cout << "\n[Step 2: Materials] Selected container material\n";
    std::cout << "  Material: " << material.material << "\n";
    std::cout << "  Tensile:  " << material.tensile_strength_MPa << " MPa\n";
    std::cout << "  Yield:    " << material.yield_strength_MPa << " MPa\n";
    std::cout << "  Note:     Excellent corrosion resistance\n";
    
    // Step 3: Failure analysis under pressure
    double internal_pressure_MPa = 200.0;  // 2000 bar
    double temperature_K = 373.15;         // 100°C
    
    std::cout << "\n[Step 3: Failure Analysis] Operating conditions\n";
    std::cout << "  Pressure:     " << internal_pressure_MPa << " MPa\n";
    std::cout << "  Temperature:  " << temperature_K << " K (" 
              << (temperature_K - 273.15) << "°C)\n";
    
    auto analysis = sim.analyze_failure(material, internal_pressure_MPa, temperature_K);
    
    std::cout << "\n[Result]\n";
    std::cout << "  Safety Factor: " << std::fixed << std::setprecision(2) 
              << analysis.safety_factor << "\n";
    std::cout << "  Status:        " << (analysis.will_fail ? "⚠️  UNSAFE" : "✅ SAFE") << "\n";
    std::cout << "  Failure Mode:  " << analysis.failure_mode << "\n";
    
    if (analysis.safety_factor >= 1.5) {
        std::cout << "  Recommendation: ✅ Approved for use\n";
    } else if (analysis.safety_factor >= 1.0) {
        std::cout << "  Recommendation: ⚠️  Marginal - add safety monitoring\n";
    } else {
        std::cout << "  Recommendation: ❌ Reject - select stronger material\n";
    }
}

// ============================================================================
// Demo 3: Data Pipe Integration (Reactive Flow)
// ============================================================================

void demo_reactive_data_flow() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Demo 3: Reactive Data Pipes (VSEPR → GUI → Materials)        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    // Create data pipes
    auto vsepr_pipe = std::make_shared<DataPipe<Molecule>>("vsepr_output");
    auto material_pipe = std::make_shared<DataPipe<std::string>>("material_recommendation");
    
    std::cout << "\n[Setup] Created reactive data pipes:\n";
    std::cout << "  1. vsepr_output (Molecule)\n";
    std::cout << "  2. material_recommendation (std::string)\n";
    
    // Subscribe to VSEPR output
    vsepr_pipe->subscribe([&](const Molecule& mol) {
        std::cout << "\n[Pipe Event] Received molecule with " << mol.num_atoms() << " atoms\n";
        
        // Analyze and recommend
        MetallicSimulator sim;
        std::string recommendation;
        
        if (mol.num_atoms() > 20) {
            recommendation = "Hastelloy C-276 (high durability needed)";
        } else if (mol.num_atoms() > 10) {
            recommendation = "Hastelloy C-22 (balanced performance)";
        } else {
            recommendation = "Steel 316L (general purpose)";
        }
        
        std::cout << "  → Recommended: " << recommendation << "\n";
        material_pipe->push(recommendation);
    });
    
    // Subscribe to material recommendations
    material_pipe->subscribe([](const std::string& rec) {
        std::cout << "[GUI Update] Material recommendation updated: " << rec << "\n";
    });
    
    // Simulate workflow
    std::cout << "\n[Simulation] Creating test molecules...\n";
    
    Molecule small_mol;
    small_mol.add_atom(1, 0.0, 0.0, 0.0);
    small_mol.add_atom(1, 0.74, 0.0, 0.0);
    std::cout << "\n→ Pushing H₂ molecule (2 atoms)";
    vsepr_pipe->push(small_mol);
    
    Molecule medium_mol;
    for (int i = 0; i < 15; i++) {
        medium_mol.add_atom(6, i * 1.5, 0.0, 0.0);
    }
    std::cout << "\n→ Pushing C₁₅ chain (15 atoms)";
    vsepr_pipe->push(medium_mol);
    
    Molecule large_mol;
    for (int i = 0; i < 30; i++) {
        large_mol.add_atom(6, i * 1.5, 0.0, 0.0);
    }
    std::cout << "\n→ Pushing C₃₀ chain (30 atoms)";
    vsepr_pipe->push(large_mol);
}

// ============================================================================
// Demo 4: Batch Molecular Processing
// ============================================================================

void demo_batch_molecular_processing() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Demo 4: Batch Molecular Processing (Production Workflow)      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    MetallicSimulator sim;
    
    struct MoleculeSpec {
        std::string name;
        std::string formula;
        int metal_z;
        double stress_requirement_MPa;
    };
    
    std::vector<MoleculeSpec> molecules = {
        {"Ni-Catalyst", "Ni(CO)₄", 28, 150.0},
        {"Cr-Complex", "Cr(CO)₆", 24, 200.0},
        {"Mo-Catalyst", "Mo(CO)₆", 42, 250.0},
        {"Fe-Complex", "Fe(CO)₅", 26, 180.0}
    };
    
    std::cout << "\n[Batch Analysis] Processing " << molecules.size() << " organometallic complexes\n\n";
    
    std::cout << std::setw(15) << std::left << "Molecule"
              << " | " << std::setw(12) << "Metal"
              << " | " << std::setw(10) << "Req. (MPa)"
              << " | " << std::setw(20) << "Recommended Material"
              << " | " << "Safety\n";
    std::cout << std::string(90, '-') << "\n";
    
    for (const auto& mol_spec : molecules) {
        // Select material based on metal type
        std::string material_name;
        if (mol_spec.metal_z == 28 || mol_spec.metal_z == 24) {
            material_name = "Hastelloy C-276";
        } else if (mol_spec.metal_z == 42) {
            material_name = "Hastelloy C-4";
        } else {
            material_name = "Hastelloy B-2";
        }
        
        auto material = sim.get_material(material_name);
        auto analysis = sim.analyze_failure(material, mol_spec.stress_requirement_MPa, 298.15);
        
        std::string safety_status = analysis.safety_factor >= 2.0 ? "✅ Excellent" :
                                    analysis.safety_factor >= 1.5 ? "✅ Good" :
                                    analysis.safety_factor >= 1.0 ? "⚠️  Marginal" : "❌ Unsafe";
        
        std::cout << std::setw(15) << std::left << mol_spec.name
                  << " | " << std::setw(12) << mol_spec.formula
                  << " | " << std::setw(10) << mol_spec.stress_requirement_MPa
                  << " | " << std::setw(20) << material_name
                  << " | " << safety_status << " (SF=" << std::fixed << std::setprecision(2) 
                  << analysis.safety_factor << ")\n";
    }
}

// ============================================================================
// Demo 5: Subsystem Registration (Full Integration)
// ============================================================================

void demo_subsystem_registration() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ Demo 5: Subsystem Registration (Full VSEPR Integration)       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    auto& interface = SubsystemInterface::instance();
    
    // Register VSEPR analyzer
    interface.register_subsystem("vsepr_analyzer", [](const std::string& input) {
        return "VSEPR Analysis: Molecular geometry computed for " + input;
    });
    
    // Register materials selector
    interface.register_subsystem("materials_selector", [](const std::string& input) {
        MetallicSimulator sim;
        if (input.find("Ni") != std::string::npos) {
            return "Recommended: Hastelloy C-276 (Ni-based alloy)";
        } else if (input.find("Cr") != std::string::npos) {
            return "Recommended: Hastelloy G-30 (Cr-resistant)";
        }
        return "Recommended: Steel 316L (general purpose)";
    });
    
    // Register failure analyzer
    interface.register_subsystem("failure_analyzer", [](const std::string& input) {
        MetallicSimulator sim;
        auto material = sim.get_material("Hastelloy C-276");
        auto analysis = sim.analyze_failure(material, 200.0, 298.15);
        
        std::ostringstream oss;
        oss << "Safety Factor: " << analysis.safety_factor << " | ";
        oss << (analysis.will_fail ? "UNSAFE" : "SAFE");
        return oss.str();
    });
    
    std::cout << "\n[Registered Subsystems]\n";
    auto subsystems = interface.list_subsystems();
    for (const auto& name : subsystems) {
        std::cout << "  ✓ " << name << "\n";
    }
    
    // Demonstrate integrated workflow
    std::cout << "\n[Integrated Workflow]\n";
    
    std::cout << "\nStep 1: VSEPR Analysis\n";
    std::cout << "  " << interface.call_subsystem("vsepr_analyzer", "Ni(CO)4") << "\n";
    
    std::cout << "\nStep 2: Materials Selection\n";
    std::cout << "  " << interface.call_subsystem("materials_selector", "Ni-based catalyst") << "\n";
    
    std::cout << "\nStep 3: Failure Analysis\n";
    std::cout << "  " << interface.call_subsystem("failure_analyzer", "pressure_test") << "\n";
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    std::cout << "█                                                              █\n";
    std::cout << "█  VSEPR-Sim ↔ Materials Subsystem Integration                █\n";
    std::cout << "█  Weaving Back to Original Code                              █\n";
    std::cout << "█                                                              █\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    
    try {
        demo_organometallic_catalyst();
        demo_stress_testing_workflow();
        demo_reactive_data_flow();
        demo_batch_molecular_processing();
        demo_subsystem_registration();
        
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║ All Integration Demos Completed Successfully! ✅              ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
