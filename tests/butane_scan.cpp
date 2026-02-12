/*
butane_scan.cpp
---------------
Dihedral scan of butane central C-C-C-C torsion.
NO minimizer - just rigid rotation to measure pure torsion energy.

This is the sanity test: sweep φ and plot ΔE(φ).
Expected: ~3.5 kcal/mol barrier between anti and gauche.
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>

using namespace vsepr;

int main() {
    std::cout << "===================================================\n";
    std::cout << "Butane Dihedral Scan (Rigid Rotation)\n";
    std::cout << "Testing pure torsion energy vs. C-C-C-C angle\n";
    std::cout << "===================================================\n\n";

    // Build butane in anti conformation
    Molecule mol;
    
    // Four carbons: C1-C2-C3-C4
    mol.add_atom(6, 0.0, 0.0, 0.0);      // C1 (0)
    mol.add_atom(6, 1.54, 0.0, 0.0);     // C2 (1)
    mol.add_atom(6, 2.31, 1.26, 0.0);    // C3 (2)
    mol.add_atom(6, 3.85, 1.26, 0.0);    // C4 (3)
    
    // Hydrogens on C1
    mol.add_atom(1, -0.36, -0.51, 0.89);  // H
    mol.add_atom(1, -0.36, -0.51, -0.89);
    mol.add_atom(1, -0.36, 1.03, 0.0);
    
    // Hydrogens on C2
    mol.add_atom(1, 1.90, -0.51, 0.89);
    mol.add_atom(1, 1.90, -0.51, -0.89);
    
    // Hydrogens on C3
    mol.add_atom(1, 1.95, 1.77, 0.89);
    mol.add_atom(1, 1.95, 1.77, -0.89);
    
    // Hydrogens on C4
    mol.add_atom(1, 4.21, 0.74, 0.89);
    mol.add_atom(1, 4.21, 0.74, -0.89);
    mol.add_atom(1, 4.21, 2.28, 0.0);
    
    // Bonds
    mol.add_bond(0, 1, 1);  // C1-C2
    mol.add_bond(1, 2, 1);  // C2-C3 (central bond to rotate)
    mol.add_bond(2, 3, 1);  // C3-C4
    
    // C-H bonds
    mol.add_bond(0, 4, 1); mol.add_bond(0, 5, 1); mol.add_bond(0, 6, 1);
    mol.add_bond(1, 7, 1); mol.add_bond(1, 8, 1);
    mol.add_bond(2, 9, 1); mol.add_bond(2, 10, 1);
    mol.add_bond(3, 11, 1); mol.add_bond(3, 12, 1); mol.add_bond(3, 13, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    std::cout << "Butane topology:\n";
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.bonds.size() << "\n";
    std::cout << "  Torsions: " << mol.torsions.size() << "\n";
    
    // Count torsion types
    int heavy_atom_torsions = 0;
    int h_involving_torsions = 0;
    for (const auto& t : mol.torsions) {
        bool has_h = (mol.atoms[t.i].Z == 1 || mol.atoms[t.j].Z == 1 ||
                      mol.atoms[t.k].Z == 1 || mol.atoms[t.l].Z == 1);
        if (has_h) h_involving_torsions++;
        else heavy_atom_torsions++;
    }
    std::cout << "  Heavy-atom torsions: " << heavy_atom_torsions << " (with barriers)\n";
    std::cout << "  H-involving torsions: " << h_involving_torsions << " (V=0)\n\n";
    
    // Create energy model with ONLY torsions enabled
    NonbondedParams nb_params;
    nb_params.epsilon = 0.0;  // Disable nonbonded
    
    EnergyModel energy_torsion_only(mol, 300.0, false, false, nb_params, true);
    
    // Find central C-C-C-C torsion
    int central_idx = -1;
    for (size_t i = 0; i < mol.torsions.size(); ++i) {
        const auto& t = mol.torsions[i];
        if (t.i == 0 && t.j == 1 && t.k == 2 && t.l == 3) {
            central_idx = i;
            break;
        }
    }
    
    if (central_idx < 0) {
        std::cerr << "ERROR: Central C1-C2-C3-C4 torsion not found!\n";
        return 1;
    }
    
    std::cout << "Central torsion: " << mol.torsions[central_idx].i << "-"
              << mol.torsions[central_idx].j << "-"
              << mol.torsions[central_idx].k << "-"
              << mol.torsions[central_idx].l << "\n\n";
    
    // Scan dihedral angle from -180° to +180°
    std::ofstream out("butane_scan.dat");
    out << "# Butane C-C-C-C dihedral scan\n";
    out << "# Angle(deg)  Energy(kcal/mol)  RelativeEnergy(kcal/mol)\n";
    
    std::vector<double> scan_coords = mol.coords;
    
    // Get initial geometry parameters for C2-C3 bond
    Vec3 r2 = get_pos(scan_coords, 1);  // C2
    Vec3 r3 = get_pos(scan_coords, 2);  // C3
    Vec3 bond_axis = (r3 - r2).normalized();
    
    double min_energy = 1e10;
    std::vector<std::pair<double, double>> scan_results;
    
    for (int step = 0; step <= 72; ++step) {  // 5° increments
        double target_angle_deg = -180.0 + step * 5.0;
        double target_angle_rad = target_angle_deg * M_PI / 180.0;
        
        // Rotate C3 and C4 + their H atoms around C2-C3 axis
        // Keep C1, C2 and their H atoms fixed
        
        // Use Rodrigues rotation formula
        double cos_theta = std::cos(target_angle_rad);
        double sin_theta = std::sin(target_angle_rad);
        
        // Reset to original geometry and apply rotation
        scan_coords = mol.coords;
        
        // Atoms to rotate: C3(2), C4(3), and H on C3(9,10), H on C4(11,12,13)
        std::vector<int> atoms_to_rotate = {2, 3, 9, 10, 11, 12, 13};
        
        for (int atom_idx : atoms_to_rotate) {
            Vec3 r = get_pos(mol.coords, atom_idx);
            Vec3 p = r - r2;  // Position relative to rotation center (C2)
            
            // Rodrigues: v_rot = v*cos(θ) + (k × v)*sin(θ) + k*(k·v)*(1-cos(θ))
            Vec3 v_rot = p * cos_theta + bond_axis.cross(p) * sin_theta + 
                         bond_axis * (bond_axis.dot(p)) * (1.0 - cos_theta);
            
            set_pos(scan_coords, atom_idx, r2 + v_rot);
        }
        
        // Measure current dihedral angle
        double actual_phi = torsion(scan_coords, 0, 1, 2, 3);
        double actual_phi_deg = actual_phi * 180.0 / M_PI;
        
        // Evaluate energy (torsion only)
        auto breakdown = energy_torsion_only.evaluate_detailed(scan_coords);
        double E = breakdown.torsion_energy;
        
        scan_results.push_back({actual_phi_deg, E});
        min_energy = std::min(min_energy, E);
    }
    
    // Print results relative to minimum
    std::cout << "Angle(deg)  E_torsion(kcal/mol)  ΔE(kcal/mol)  Conformation\n";
    std::cout << "-----------------------------------------------------------\n";
    
    for (const auto& [angle, E] : scan_results) {
        double rel_E = E - min_energy;
        out << std::fixed << std::setprecision(1) << angle << "  "
            << std::setprecision(6) << E << "  " << rel_E << "\n";
        
        // Print key conformations
        if (std::abs(angle - 180.0) < 6.0 || std::abs(angle + 180.0) < 6.0) {
            std::cout << std::setw(10) << std::fixed << std::setprecision(1) << angle << "  "
                      << std::setw(12) << std::setprecision(6) << E << "  "
                      << std::setw(12) << rel_E << "  anti (stable)\n";
        } else if (std::abs(angle - 60.0) < 6.0) {
            std::cout << std::setw(10) << angle << "  "
                      << std::setw(12) << E << "  "
                      << std::setw(12) << rel_E << "  gauche+\n";
        } else if (std::abs(angle + 60.0) < 6.0) {
            std::cout << std::setw(10) << angle << "  "
                      << std::setw(12) << E << "  "
                      << std::setw(12) << rel_E << "  gauche-\n";
        } else if (std::abs(angle) < 6.0) {
            std::cout << std::setw(10) << angle << "  "
                      << std::setw(12) << E << "  "
                      << std::setw(12) << rel_E << "  eclipsed (unstable)\n";
        }
    }
    
    out.close();
    
    std::cout << "\n===================================================\n";
    std::cout << "Scan complete! Data written to butane_scan.dat\n";
    std::cout << "Expected barrier: ~3-4 kcal/mol between anti and gauche\n";
    std::cout << "Expected: anti (180°) most stable, eclipsed (0°) highest\n";
    std::cout << "===================================================\n";
    
    return 0;
}
