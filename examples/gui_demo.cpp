/**
 * VSEPR-Sim GUI Example: Context Menus + Data Piping
 * Demonstrates right-click menus and reactive data flow
 */

#include "gui/context_menu.hpp"
#include "gui/data_pipe.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace vsepr::gui;

// Example: Molecule data structure for demo
struct DemoMoleculeData {
    std::string id;
    std::string formula;
    double energy;
    int atom_count;
    int bond_count;
};

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VSEPR-Sim GUI Demo: Context Menus + Data Piping             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    // ═══════════════════════════════════════════════════════════════════════
    // PART 1: Context Menu Demo
    // ═══════════════════════════════════════════════════════════════════════
    
    std::cout << "══════ PART 1: Context Menu Examples ══════\n\n";
    
    // Example 1: Molecule context menu
    {
        std::cout << "1. Molecule Context Menu:\n";
        auto menu = MoleculeContextMenu::build(
            "mol_001",           // ID
            "H₃N",              // Formula
            -45.2,              // Energy
            4,                  // Atoms
            3                   // Bonds
        );
        
        ContextMenuManager::instance().show(menu, 100, 200);
    }
    
    // Example 2: Atom context menu
    {
        std::cout << "2. Atom Context Menu:\n";
        auto menu = AtomContextMenu::build(
            0,                  // Atom index
            "N",                // Element
            0.5, 1.2, -0.3,    // x, y, z
            -0.45               // Charge
        );
        
        ContextMenuManager::instance().show(menu, 250, 300);
    }
    
    // Example 3: Bond context menu
    {
        std::cout << "3. Bond Context Menu:\n";
        auto menu = BondContextMenu::build(
            2,                  // Bond index
            0, 1,               // Atom indices
            1.5,                // Bond order
            1.012               // Length (Angstroms)
        );
        
        ContextMenuManager::instance().show(menu, 400, 150);
    }
    
    // Example 4: Plot context menu
    {
        std::cout << "4. Plot Context Menu:\n";
        auto menu = PlotContextMenu::build(
            "Energy vs. Time",  // Plot type
            true,               // Show grid
            true,               // Show legend
            "energy_plot"       // Export path
        );
        
        ContextMenuManager::instance().show(menu, 500, 250);
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // PART 2: Data Piping Demo
    // ═══════════════════════════════════════════════════════════════════════
    
    std::cout << "\n══════ PART 2: Data Piping Examples ══════\n\n";
    
    // Create data pipes
    auto molecule_pipe = std::make_shared<DataPipe<DemoMoleculeData>>("molecule_data");
    auto energy_pipe = std::make_shared<DataPipe<double>>("energy");
    auto status_pipe = std::make_shared<DataPipe<std::string>>("status");
    
    // Register pipes
    PipeNetwork::instance().registerPipe("molecule_data", molecule_pipe);
    PipeNetwork::instance().registerPipe("energy", energy_pipe);
    PipeNetwork::instance().registerPipe("status", status_pipe);
    
    // Subscribe to molecule data
    molecule_pipe->subscribe([](const DemoMoleculeData& mol) {
        std::cout << "[MOLECULE] Updated: " << mol.formula 
                  << " (E=" << mol.energy << " kcal/mol)\n";
    });
    
    // Transform: molecule → energy
    auto energy_from_molecule = molecule_pipe->transform<double>(
        "energy_from_molecule",
        [](const DemoMoleculeData& mol) { return mol.energy; }
    );
    
    energy_from_molecule->subscribe([](double energy) {
        std::cout << "[ENERGY] " << energy << " kcal/mol\n";
    });
    
    // Filter: only stable molecules (E < 0)
    auto stable_molecules = molecule_pipe->filter(
        "stable_only",
        [](const DemoMoleculeData& mol) { return mol.energy < 0.0; }
    );
    
    stable_molecules->subscribe([](const DemoMoleculeData& mol) {
        std::cout << "[STABLE] " << mol.formula << " is stable!\n";
    });
    
    // Subscribe to status updates
    status_pipe->subscribe([](const std::string& status) {
        std::cout << "[STATUS] " << status << "\n";
    });
    
    std::cout << "\nPushing data through pipes...\n\n";
    
    // Push test data
    status_pipe->push("Initializing...");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Molecule 1: Ammonia (stable)
    DemoMoleculeData ammonia{"mol_001", "H₃N", -45.2, 4, 3};
    molecule_pipe->push(ammonia);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Molecule 2: Unstable intermediate (filtered out)
    DemoMoleculeData unstable{"mol_002", "H₂O₂", 12.5, 4, 3};
    molecule_pipe->push(unstable);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Molecule 3: Water (stable)
    DemoMoleculeData water{"mol_003", "H₂O", -57.8, 3, 2};
    molecule_pipe->push(water);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    status_pipe->push("Complete!");
    
    // ═══════════════════════════════════════════════════════════════════════
    // PART 3: Pipe Network Info
    // ═══════════════════════════════════════════════════════════════════════
    
    std::cout << "\n══════ PART 3: Pipe Network Status ══════\n\n";
    
    auto pipe_info = PipeNetwork::instance().getPipeInfo();
    
    std::cout << "Registered Pipes:\n";
    for (const auto& info : pipe_info) {
        std::cout << "  • " << info.name 
                  << " (" << (info.connected ? "CONNECTED" : "IDLE") << ")\n";
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // PART 4: Integration Example
    // ═══════════════════════════════════════════════════════════════════════
    
    std::cout << "\n══════ PART 4: Integration Example ══════\n\n";
    std::cout << "Right-click on molecule → Context menu → Export\n";
    std::cout << "Molecule data flows through pipe → Subscribers notified\n";
    std::cout << "UI updates automatically (reactive data flow)\n\n";
    
    // Simulate right-click export action
    auto export_action = [&water, &status_pipe]() {
        std::cout << "\n[ACTION] User clicked 'Export XYZ'...\n";
        
        // Push status update through pipe
        status_pipe->push("Exporting " + water.formula + "...");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Simulate export
        std::cout << "[EXPORT] Writing " << water.id << ".xyz\n";
        std::cout << "[EXPORT] 3 atoms, 2 bonds\n";
        std::cout << "[EXPORT] Energy: " << water.energy << " kcal/mol\n";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        status_pipe->push("Export complete!");
    };
    
    // Build menu with export action
    ContextMenu export_menu;
    export_menu.addAction("Export XYZ", export_action, "Ctrl+E");
    export_menu.addInfo("Formula", water.formula);
    export_menu.addInfo("Energy", std::to_string(water.energy) + " kcal/mol");
    
    std::cout << "Context menu for " << water.formula << ":\n";
    ContextMenuManager::instance().show(export_menu, 300, 200);
    
    // Execute export action
    export_action();
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Demo Complete! ✅                                             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
