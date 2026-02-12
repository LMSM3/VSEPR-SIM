/**
 * conformer_test.cpp
 * ------------------
 * Simple test for ConformerFinder implementation
 */

#include "sim/molecule.hpp"
#include "sim/molecule_builder.hpp"
#include "sim/optimizer.hpp"
#include "sim/conformer_finder.hpp"
#include "pot/energy_model.hpp"
#include <iostream>
#include <iomanip>

using namespace vsepr;

int main() {
    std::cout << "\n=== ConformerFinder Test ===\n\n";
    
    // Load periodic table
    auto pt = PeriodicTable::load_separated(
        "data/elements.physics.json",
        "data/elements.visual.json"
    );
    std::cout << "Loaded periodic table.\n\n";
    
    // Build butane (C4H10) - manually for testing
    std::cout << "Building butane (C4H10) manually...\n";
    Molecule mol;
    
    // Add atoms with rough coordinates
    // C0-C1-C2-C3 chain
    mol.add_atom(6, 0.0, 0.0, 0.0);  // C0
    mol.add_atom(6, 1.5, 0.0, 0.0);  // C1
    mol.add_atom(6, 3.0, 0.0, 0.0);  // C2
    mol.add_atom(6, 4.5, 0.0, 0.0);  // C3
    
    // Hydrogens
    for (int i = 0; i < 10; ++i) {
        mol.add_atom(1, i*0.5, 1.0, 0.0);
    }
    
    // Add C-C bonds
    mol.add_bond(0, 1, 1);  // C0-C1
    mol.add_bond(1, 2, 1);  // C1-C2 (rotatable!)
    mol.add_bond(2, 3, 1);  // C2-C3
    
    // Add C-H bonds
    mol.add_bond(0, 4, 1);  // C0-H
    mol.add_bond(0, 5, 1);
    mol.add_bond(0, 6, 1);
    mol.add_bond(1, 7, 1);  // C1-H
    mol.add_bond(1, 8, 1);
    mol.add_bond(2, 9, 1);  // C2-H
    mol.add_bond(2, 10, 1);
    mol.add_bond(3, 11, 1); // C3-H
    mol.add_bond(3, 12, 1);
    mol.add_bond(3, 13, 1);
    
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.num_bonds() << "\n";
    std::cout << "\n";
    
    // Setup energy model
    NonbondedParams nb_params;
    nb_params.scale_13 = 0.0;
    nb_params.scale_14 = 0.5;
    EnergyModel energy(mol, 300.0, true, true, nb_params, false, false, 0.1);
    
    // Test rotatable bond detection
    std::cout << "Detecting rotatable bonds...\n";
    auto rotatable = find_rotatable_bonds(mol);
    std::cout << "  Found " << rotatable.size() << " rotatable bonds\n";
    for (const auto& r : rotatable) {
        std::cout << "    Bond " << r.i << "-" << r.j 
                  << " (dihedral: " << r.a << "-" << r.i << "-" << r.j << "-" << r.b << ")"
                  << " angle=" << std::fixed << std::setprecision(1) << (r.current_angle * 180.0 / M_PI) << "°\n";
    }
    std::cout << "\n";
    
    // Run conformer search
    std::cout << "Running conformer search (20 starts, seed=42)...\n";
    ConformerFinderSettings conf_settings;
    conf_settings.num_starts = 20;
    conf_settings.seed = 42;
    conf_settings.enable_basin_hopping = false;
    conf_settings.opt_settings.max_iterations = 500;
    conf_settings.opt_settings.tol_rms_force = 0.01;
    conf_settings.opt_settings.print_every = 0;
    
    ConformerFinder finder(conf_settings);
    auto conformers = finder.find_conformers(mol, energy);
    
    std::cout << "\nFound " << conformers.size() << " unique conformers:\n\n";
    for (size_t i = 0; i < conformers.size() && i < 10; ++i) {
        std::cout << "  " << std::setw(2) << (i+1) << ". E = " 
                  << std::fixed << std::setprecision(3) << conformers[i].energy << " kcal/mol";
        if (i > 0) {
            double delta = conformers[i].energy - conformers[0].energy;
            std::cout << " (+" << std::setprecision(2) << delta << ")";
        }
        std::cout << "\n";
    }
    
    // Test determinism
    std::cout << "\n=== Testing Determinism ===\n";
    std::cout << "Running second search with same seed (new finder instance)...\n";
    ConformerFinder finder2(conf_settings);  // New instance with same seed
    auto conformers2 = finder2.find_conformers(mol, energy);
    
    bool identical = (conformers.size() == conformers2.size());
    if (identical) {
        for (size_t i = 0; i < conformers.size(); ++i) {
            if (std::abs(conformers[i].energy - conformers2[i].energy) > 1e-6) {
                identical = false;
                break;
            }
        }
    }
    
    if (identical) {
        std::cout << "✓ PASS: Both runs produced identical results\n";
    } else {
        std::cout << "✗ FAIL: Results differ!\n";
        std::cout << "  Run 1: " << conformers.size() << " conformers\n";
        std::cout << "  Run 2: " << conformers2.size() << " conformers\n";
    }
    
    std::cout << "\n=== Test Complete ===\n\n";
    
    return identical ? 0 : 1;
}
