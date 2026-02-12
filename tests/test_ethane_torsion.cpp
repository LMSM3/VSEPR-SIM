/**
 * test_ethane_torsion.cpp
 * =======================
 * Critical test: Ethane C-C torsional barrier
 * 
 * Ethane (H3C-CH3) is THE definitive torsion test because:
 * - No angle strain (all tetrahedral)
 * - No electrostatics (all C-H bonds)
 * - No VSEPR effects
 * - Only torsional energy matters
 * 
 * Expected behavior:
 * - Staggered conformations (60°, 180°, -60°) = MINIMA
 * - Eclipsed conformations (0°, 120°, -120°) = MAXIMA
 * - Barrier height: ~2.9 kcal/mol (experimental)
 * 
 * Test: Scan H-C-C-H dihedral from 0° to 360°
 */

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

using namespace vsepr;

// Build ethane with specific H-C-C-H dihedral angle
Molecule build_ethane(double dihedral_deg) {
    Molecule mol;
    
    double dihedral = dihedral_deg * M_PI / 180.0;
    
    // C1 at origin
    mol.add_atom(6, 0.0, 0.0, 0.0);
    
    // C2 along x-axis at typical C-C bond length
    double r_cc = 1.54;  // Å
    mol.add_atom(6, r_cc, 0.0, 0.0);
    
    // H atoms on C1 (staggered by default)
    double r_ch = 1.09;  // Å
    double tet = 109.47 * M_PI / 180.0;  // tetrahedral angle
    
    // C1 hydrogens (in yz plane for simplicity)
    mol.add_atom(1, -r_ch * std::cos(tet/2), r_ch * std::sin(tet/2), 0.0);
    mol.add_atom(1, -r_ch * std::cos(tet/2), -r_ch * std::sin(tet/2) * std::cos(M_PI/3), r_ch * std::sin(tet/2) * std::sin(M_PI/3));
    mol.add_atom(1, -r_ch * std::cos(tet/2), -r_ch * std::sin(tet/2) * std::cos(M_PI/3), -r_ch * std::sin(tet/2) * std::sin(M_PI/3));
    
    // C2 hydrogens (rotated by dihedral angle)
    double x_base = r_cc + r_ch * std::cos(tet/2);
    double r_perp = r_ch * std::sin(tet/2);
    
    mol.add_atom(1, x_base, r_perp * std::cos(dihedral), r_perp * std::sin(dihedral));
    mol.add_atom(1, x_base, r_perp * std::cos(dihedral + 2*M_PI/3), r_perp * std::sin(dihedral + 2*M_PI/3));
    mol.add_atom(1, x_base, r_perp * std::cos(dihedral + 4*M_PI/3), r_perp * std::sin(dihedral + 4*M_PI/3));
    
    // Add bonds
    mol.add_bond(0, 1, 1);  // C-C
    mol.add_bond(0, 2, 1);  // C1-H
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.add_bond(1, 5, 1);  // C2-H
    mol.add_bond(1, 6, 1);
    mol.add_bond(1, 7, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();  // CRITICAL: Must generate torsions!
    
    return mol;
}

// Compute H-C-C-H dihedral angle
double compute_dihedral(const std::vector<double>& coords, int i, int j, int k, int l) {
    Vec3 r1(coords[3*i], coords[3*i+1], coords[3*i+2]);
    Vec3 r2(coords[3*j], coords[3*j+1], coords[3*j+2]);
    Vec3 r3(coords[3*k], coords[3*k+1], coords[3*k+2]);
    Vec3 r4(coords[3*l], coords[3*l+1], coords[3*l+2]);
    
    Vec3 b1 = r2 - r1;
    Vec3 b2 = r3 - r2;
    Vec3 b3 = r4 - r3;
    
    Vec3 n1 = b1.cross(b2);
    Vec3 n2 = b2.cross(b3);
    
    double cos_phi = n1.dot(n2) / (n1.norm() * n2.norm());
    cos_phi = std::max(-1.0, std::min(1.0, cos_phi));
    
    Vec3 b2_hat = b2 / b2.norm();
    double sin_phi = b2_hat.dot(n1.cross(n2)) / (n1.norm() * n2.norm());
    
    double phi = std::atan2(sin_phi, cos_phi);
    return phi * 180.0 / M_PI;
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "Ethane Torsional Barrier Test\n";
    std::cout << "==========================================\n\n";
    
    std::cout << "Scanning H-C-C-H dihedral angle from 0° to 360°\n";
    std::cout << "Expected: Minima at 60°, 180°, 300° (staggered)\n";
    std::cout << "          Maxima at 0°, 120°, 240° (eclipsed)\n";
    std::cout << "          Barrier: ~2-3 kcal/mol\n\n";
    
    std::cout << std::setw(10) << "Angle(°)"
              << std::setw(15) << "Energy"
              << std::setw(15) << "Relative"
              << std::setw(12) << "Type\n";
    std::cout << std::string(52, '-') << "\n";
    
    std::vector<double> angles;
    std::vector<double> energies;
    
    // Scan from 0 to 360 degrees
    for (int deg = 0; deg <= 360; deg += 10) {
        Molecule mol = build_ethane(deg);
        
        // CRITICAL: Enable torsions! (disabled by default in EnergyModel)
        EnergyModel energy(mol, 
                          300.0,    // bond_k
                          false,    // use_angles
                          true,     // use_nonbonded
                          NonbondedParams(),
                          true);    // use_torsions = TRUE!
        
        // Debug: print torsion count on first iteration
        if (deg == 0) {
            std::cout << "DEBUG: Molecule has " << mol.atoms.size() << " atoms\n";
            std::cout << "DEBUG: Molecule has " << mol.bonds.size() << " bonds\n";
            std::cout << "DEBUG: Molecule has " << mol.torsions.size() << " torsions\n";
            std::cout << "DEBUG: Torsions (i-j-k-l):\n";
            for (const auto& t : mol.torsions) {
                std::cout << "  " << t.i << "-" << t.j << "-" << t.k << "-" << t.l 
                         << " (Z: " << mol.atoms[t.i].Z << "-" << mol.atoms[t.j].Z 
                         << "-" << mol.atoms[t.k].Z << "-" << mol.atoms[t.l].Z << ")\n";
            }
            
            // Check what multiplicity the energy model computed
            std::cout << "\nDEBUG: Checking first torsion parameters...\n";
            std::cout << "(Need to access energy model internals - skipping for now)\n";
            std::cout << "\n";
        }
        
        std::vector<double> grad(mol.coords.size(), 0.0);
        double E = energy.evaluate_energy_gradient(mol.coords, grad);
        
        angles.push_back(deg);
        energies.push_back(E);
    }
    
    // Find minimum energy (should be staggered)
    double E_min = *std::min_element(energies.begin(), energies.end());
    
    // Print results
    for (size_t i = 0; i < angles.size(); ++i) {
        double rel_E = energies[i] - E_min;
        
        std::string type = "";
        int deg = (int)angles[i] % 360;
        
        // Expected minima (staggered): 60, 180, 300
        if (deg == 60 || deg == 180 || deg == 300) {
            type = "MIN?";
        }
        // Expected maxima (eclipsed): 0, 120, 240
        else if (deg == 0 || deg == 120 || deg == 240 || deg == 360) {
            type = "MAX?";
        }
        
        std::cout << std::setw(10) << deg
                  << std::setw(15) << std::fixed << std::setprecision(4) << energies[i]
                  << std::setw(15) << std::fixed << std::setprecision(4) << rel_E
                  << std::setw(12) << type << "\n";
    }
    
    // Find actual minima and maxima
    std::cout << "\n==========================================\n";
    std::cout << "Analysis:\n";
    std::cout << "==========================================\n";
    
    double E_max = *std::max_element(energies.begin(), energies.end());
    double barrier = E_max - E_min;
    
    std::cout << "Minimum energy: " << E_min << " kcal/mol\n";
    std::cout << "Maximum energy: " << E_max << " kcal/mol\n";
    std::cout << "Barrier height: " << barrier << " kcal/mol";
    
    if (barrier < 0.5) {
        std::cout << " ❌ FAIL: Barrier too small (torsions not working)\n";
    } else if (barrier < 2.0) {
        std::cout << " ⚠️  WARNING: Barrier low (should be ~2-3 kcal/mol)\n";
    } else if (barrier > 4.0) {
        std::cout << " ⚠️  WARNING: Barrier high (should be ~2-3 kcal/mol)\n";
    } else {
        std::cout << " ✅ PASS: Barrier in reasonable range\n";
    }
    
    // Check if minima are at right places
    std::cout << "\nMinima locations:\n";
    for (size_t i = 0; i < angles.size(); ++i) {
        if (energies[i] - E_min < 0.1) {  // Within 0.1 kcal/mol of minimum
            std::cout << "  " << angles[i] << "°";
            int deg = (int)angles[i] % 360;
            if (deg == 60 || deg == 180 || deg == 300) {
                std::cout << " ✅ (staggered - correct)\n";
            } else {
                std::cout << " ❌ (should be 60°, 180°, or 300°)\n";
            }
        }
    }
    
    std::cout << "\nMaxima locations:\n";
    for (size_t i = 0; i < angles.size(); ++i) {
        if (E_max - energies[i] < 0.1) {  // Within 0.1 kcal/mol of maximum
            std::cout << "  " << angles[i] << "°";
            int deg = (int)angles[i] % 360;
            if (deg == 0 || deg == 120 || deg == 240 || deg == 360) {
                std::cout << " ✅ (eclipsed - correct)\n";
            } else {
                std::cout << " ❌ (should be 0°, 120°, or 240°)\n";
            }
        }
    }
    
    return (barrier < 0.5) ? 1 : 0;
}
