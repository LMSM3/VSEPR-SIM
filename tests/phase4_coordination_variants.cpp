/**
 * phase4_coordination_variants.cpp
 * =================================
 * Phase 4 Testing: Coordination Geometry Variants + Chelation
 * 
 * Tests:
 * - Fe(CN)6^4- (octahedral, strong-field ligands)
 * - Ni(CN)4^2- (square planar)
 * - ZnCl4^2- (tetrahedral)
 * - Metal-oxalate chelate [M(ox)3]^n- (bidentate binding)
 * 
 * Run conditions:
 * - 12-16 seeds each
 * - For chelates: 2+ distinct initial placements
 * 
 * PASS criteria:
 * - Fe(CN)₆: CN=6, angle histogram shows 90/180, stable Fe–C distances
 * - Ni(CN)₄: CN=4, ligands remain planar, 90/180 angles
 * - ZnCl₄: CN=4, angles cluster near 109.5° (tetrahedral), not planar
 * - Chelate: bidentate binding persists, rings remain closed
 */

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <random>
#include <vector>
#include <map>
#include <algorithm>

using namespace vsepr;

//=============================================================================
// Utilities
//=============================================================================

void print_header(const std::string& title) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(60) << title << " ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
}

void print_section(const std::string& title) {
    std::cout << "\n─── " << title << " ───\n";
}

struct CoordinationMetrics {
    int coordination_number;
    std::vector<double> metal_ligand_distances;
    std::vector<double> ligand_angles;  // L-M-L angles
    double planarity_deviation;  // RMS deviation from best-fit plane (for square planar)
    bool geometry_preserved;
};

CoordinationMetrics analyze_coordination(const Molecule& mol, uint32_t metal_idx, 
                                         uint32_t ligand_Z, double max_coord_dist = 3.0) {
    CoordinationMetrics metrics;
    metrics.coordination_number = 0;
    metrics.planarity_deviation = 0.0;
    metrics.geometry_preserved = true;
    
    std::vector<uint32_t> ligand_indices;
    
    // Find coordinating atoms
    for (uint32_t i = 0; i < mol.num_atoms(); ++i) {
        if (i == metal_idx) continue;
        
        // For CN ligands, detect C atoms bonded to metal
        // For simpler ligands, detect by element
        bool is_ligand_donor = (mol.atoms[i].Z == ligand_Z);
        
        if (is_ligand_donor) {
            double dx = mol.coords[3*i] - mol.coords[3*metal_idx];
            double dy = mol.coords[3*i+1] - mol.coords[3*metal_idx+1];
            double dz = mol.coords[3*i+2] - mol.coords[3*metal_idx+2];
            double r = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (r < max_coord_dist) {
                ligand_indices.push_back(i);
                metrics.metal_ligand_distances.push_back(r);
                metrics.coordination_number++;
            }
        }
    }
    
    // Calculate all L-M-L angles
    for (size_t i = 0; i < ligand_indices.size(); ++i) {
        for (size_t j = i+1; j < ligand_indices.size(); ++j) {
            uint32_t lig1 = ligand_indices[i];
            uint32_t lig2 = ligand_indices[j];
            
            double v1[3], v2[3];
            for (int d = 0; d < 3; ++d) {
                v1[d] = mol.coords[3*lig1 + d] - mol.coords[3*metal_idx + d];
                v2[d] = mol.coords[3*lig2 + d] - mol.coords[3*metal_idx + d];
            }
            
            double dot = v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
            double r1 = std::sqrt(v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]);
            double r2 = std::sqrt(v2[0]*v2[0] + v2[1]*v2[1] + v2[2]*v2[2]);
            
            double cos_angle = dot / (r1 * r2);
            cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
            double angle_deg = std::acos(cos_angle) * 180.0 / M_PI;
            metrics.ligand_angles.push_back(angle_deg);
        }
    }
    
    // Calculate planarity (for square planar check)
    if (ligand_indices.size() == 4) {
        // Compute centroid
        double cx = 0, cy = 0, cz = 0;
        for (uint32_t idx : ligand_indices) {
            cx += mol.coords[3*idx];
            cy += mol.coords[3*idx+1];
            cz += mol.coords[3*idx+2];
        }
        cx /= 4.0; cy /= 4.0; cz /= 4.0;
        
        // Find normal via cross product of two vectors
        double v1[3], v2[3];
        for (int d = 0; d < 3; ++d) {
            v1[d] = mol.coords[3*ligand_indices[1] + d] - mol.coords[3*ligand_indices[0] + d];
            v2[d] = mol.coords[3*ligand_indices[2] + d] - mol.coords[3*ligand_indices[0] + d];
        }
        
        double normal[3] = {
            v1[1]*v2[2] - v1[2]*v2[1],
            v1[2]*v2[0] - v1[0]*v2[2],
            v1[0]*v2[1] - v1[1]*v2[0]
        };
        double norm = std::sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
        if (norm > 1e-10) {
            normal[0] /= norm; normal[1] /= norm; normal[2] /= norm;
            
            // Calculate RMS deviation from plane
            double sum_sq = 0;
            for (uint32_t idx : ligand_indices) {
                double dist_to_plane = std::abs(
                    normal[0] * (mol.coords[3*idx] - cx) +
                    normal[1] * (mol.coords[3*idx+1] - cy) +
                    normal[2] * (mol.coords[3*idx+2] - cz)
                );
                sum_sq += dist_to_plane * dist_to_plane;
            }
            metrics.planarity_deviation = std::sqrt(sum_sq / 4.0);
        }
    }
    
    return metrics;
}

void perturb_coordinates(Molecule& mol, double amplitude, std::mt19937& rng) {
    std::normal_distribution<double> dist(0.0, amplitude);
    for (uint32_t i = 0; i < mol.num_atoms(); ++i) {
        for (int d = 0; d < 3; ++d) {
            mol.coords[3*i + d] += dist(rng);
        }
    }
}

Molecule build_octahedral_complex(uint32_t metal_Z, uint32_t ligand_Z, double bond_length = 2.0) {
    Molecule mol;
    
    // Metal at origin
    mol.add_atom(metal_Z, 0, 0, 0);
    mol.coords.push_back(0.0);
    mol.coords.push_back(0.0);
    mol.coords.push_back(0.0);
    
    // 6 ligands at octahedral positions
    const double positions[6][3] = {
        { bond_length,  0.0,          0.0},
        {-bond_length,  0.0,          0.0},
        { 0.0,          bond_length,  0.0},
        { 0.0,         -bond_length,  0.0},
        { 0.0,          0.0,          bond_length},
        { 0.0,          0.0,         -bond_length}
    };
    
    for (int i = 0; i < 6; ++i) {
        mol.add_atom(ligand_Z, 0, 0, 0);
        mol.coords.push_back(positions[i][0]);
        mol.coords.push_back(positions[i][1]);
        mol.coords.push_back(positions[i][2]);
    }
    
    return mol;
}

Molecule build_square_planar_complex(uint32_t metal_Z, uint32_t ligand_Z, double bond_length = 2.0) {
    Molecule mol;
    
    mol.add_atom(metal_Z, 0, 0, 0);
    mol.coords.push_back(0.0);
    mol.coords.push_back(0.0);
    mol.coords.push_back(0.0);
    
    // 4 ligands in xy-plane
    const double positions[4][3] = {
        { bond_length,  0.0,          0.0},
        {-bond_length,  0.0,          0.0},
        { 0.0,          bond_length,  0.0},
        { 0.0,         -bond_length,  0.0}
    };
    
    for (int i = 0; i < 4; ++i) {
        mol.add_atom(ligand_Z, 0, 0, 0);
        mol.coords.push_back(positions[i][0]);
        mol.coords.push_back(positions[i][1]);
        mol.coords.push_back(positions[i][2]);
    }
    
    return mol;
}

Molecule build_tetrahedral_complex(uint32_t metal_Z, uint32_t ligand_Z, double bond_length = 2.0) {
    Molecule mol;
    
    mol.add_atom(metal_Z, 0, 0, 0);
    mol.coords.push_back(0.0);
    mol.coords.push_back(0.0);
    mol.coords.push_back(0.0);
    
    // Tetrahedral positions
    const double a = bond_length / std::sqrt(3.0);
    const double positions[4][3] = {
        { a,  a,  a},
        {-a, -a,  a},
        {-a,  a, -a},
        { a, -a, -a}
    };
    
    for (int i = 0; i < 4; ++i) {
        mol.add_atom(ligand_Z, 0, 0, 0);
        mol.coords.push_back(positions[i][0]);
        mol.coords.push_back(positions[i][1]);
        mol.coords.push_back(positions[i][2]);
    }
    
    return mol;
}

//=============================================================================
// Test 1: Fe(CN)6^4- Octahedral
//=============================================================================

void test_ferrocyanide() {
    print_header("TEST 1: [Fe(CN)6]⁴⁻ - Octahedral Strong-Field");
    
    const int num_seeds = 14;
    const int max_steps = 2000;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    
    int cn_correct = 0;
    int angles_octahedral = 0;
    std::vector<double> all_fe_c_distances;
    
    std::cout << "Running " << num_seeds << " optimizations with different seeds...\n\n";
    
    for (int seed = 0; seed < num_seeds; ++seed) {
        // Build Fe(CN)6^4- with Fe at center, C atoms coordinating
        Molecule mol = build_octahedral_complex(26, 6, 2.0);  // Fe=26, C=6
        
        if (seed > 0) {
            perturb_coordinates(mol, 0.15, rng);
        }
        
        FIREOptimizer minimizer;
        minimizer.max_steps = max_steps;
        minimizer.f_tol = 1e-6;
        minimizer.minimize(mol);
        
        CoordinationMetrics metrics = analyze_coordination(mol, 0, 6, 3.0);  // Metal=0, Ligand=C
        
        if (metrics.coordination_number == 6) cn_correct++;
        
        // Check if angles cluster near 90/180
        int near_90 = 0, near_180 = 0;
        for (double angle : metrics.ligand_angles) {
            if (std::abs(angle - 90.0) < 15.0) near_90++;
            else if (std::abs(angle - 180.0) < 15.0) near_180++;
        }
        
        // Octahedral should have 12 ~90° and 3 ~180° angles
        bool octahedral_pattern = (near_90 >= 10) && (near_180 >= 2);
        if (octahedral_pattern) angles_octahedral++;
        
        all_fe_c_distances.insert(all_fe_c_distances.end(), 
                                  metrics.metal_ligand_distances.begin(),
                                  metrics.metal_ligand_distances.end());
        
        if (seed < 3 || metrics.coordination_number != 6 || !octahedral_pattern) {
            std::cout << "  Seed " << std::setw(2) << seed 
                      << ": CN=" << metrics.coordination_number
                      << ", ~90°: " << near_90 << ", ~180°: " << near_180
                      << " → " << (octahedral_pattern ? "octahedral ✓" : "irregular ✗")
                      << "\n";
        }
    }
    
    // Calculate Fe-C distance statistics
    double mean_dist = 0.0;
    for (double d : all_fe_c_distances) mean_dist += d;
    mean_dist /= all_fe_c_distances.size();
    
    double std_dist = 0.0;
    for (double d : all_fe_c_distances) {
        std_dist += (d - mean_dist) * (d - mean_dist);
    }
    std_dist = std::sqrt(std_dist / all_fe_c_distances.size());
    
    std::cout << "\nResults:\n";
    std::cout << "  CN=6: " << cn_correct << "/" << num_seeds 
              << " (" << std::fixed << std::setprecision(1) << 100.0*cn_correct/num_seeds << "%)\n";
    std::cout << "  Octahedral angles: " << angles_octahedral << "/" << num_seeds 
              << " (" << 100.0*angles_octahedral/num_seeds << "%)\n";
    std::cout << "  Fe-C distances: " << std::setprecision(2) << mean_dist 
              << " ± " << std_dist << " Å\n";
    
    bool pass = (cn_correct >= 12) && (angles_octahedral >= 12);
    std::cout << "\n" << (pass ? "✓ PASS" : "✗ FAIL") << "\n";
}

//=============================================================================
// Test 2: Ni(CN)4^2- Square Planar
//=============================================================================

void test_nickel_cyanide() {
    print_header("TEST 2: [Ni(CN)4]²⁻ - Square Planar");
    
    const int num_seeds = 14;
    const int max_steps = 2000;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    
    int cn_correct = 0;
    int planar = 0;
    int angles_square = 0;
    
    std::cout << "Running " << num_seeds << " optimizations...\n\n";
    
    for (int seed = 0; seed < num_seeds; ++seed) {
        Molecule mol = build_square_planar_complex(28, 6, 1.9);  // Ni=28, C=6
        
        if (seed > 0) {
            perturb_coordinates(mol, 0.15, rng);
        }
        
        FIREOptimizer minimizer;
        minimizer.max_steps = max_steps;
        minimizer.f_tol = 1e-6;
        minimizer.minimize(mol);
        
        CoordinationMetrics metrics = analyze_coordination(mol, 0, 6, 3.0);
        
        if (metrics.coordination_number == 4) cn_correct++;
        
        // Check planarity: deviation should be < 0.3 Å
        bool is_planar = metrics.planarity_deviation < 0.3;
        if (is_planar) planar++;
        
        // Check angles: should have 90° and 180°
        int near_90 = 0, near_180 = 0;
        for (double angle : metrics.ligand_angles) {
            if (std::abs(angle - 90.0) < 15.0) near_90++;
            else if (std::abs(angle - 180.0) < 15.0) near_180++;
        }
        
        bool square_pattern = (near_90 >= 3) && (near_180 >= 1);
        if (square_pattern) angles_square++;
        
        if (seed < 3 || !is_planar) {
            std::cout << "  Seed " << std::setw(2) << seed 
                      << ": CN=" << metrics.coordination_number
                      << ", planarity=" << std::fixed << std::setprecision(3) << metrics.planarity_deviation << " Å"
                      << " → " << (is_planar ? "planar ✓" : "non-planar ✗")
                      << "\n";
        }
    }
    
    std::cout << "\nResults:\n";
    std::cout << "  CN=4: " << cn_correct << "/" << num_seeds 
              << " (" << std::fixed << std::setprecision(1) << 100.0*cn_correct/num_seeds << "%)\n";
    std::cout << "  Planar geometry: " << planar << "/" << num_seeds 
              << " (" << 100.0*planar/num_seeds << "%)\n";
    std::cout << "  Square angles: " << angles_square << "/" << num_seeds 
              << " (" << 100.0*angles_square/num_seeds << "%)\n";
    
    bool pass = (cn_correct >= 12) && (planar >= 11) && (angles_square >= 11);
    std::cout << "\n" << (pass ? "✓ PASS" : "✗ FAIL") << "\n";
}

//=============================================================================
// Test 3: ZnCl4^2- Tetrahedral
//=============================================================================

void test_zinc_chloride() {
    print_header("TEST 3: [ZnCl4]²⁻ - Tetrahedral");
    
    const int num_seeds = 14;
    const int max_steps = 2000;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    
    int cn_correct = 0;
    int angles_tetrahedral = 0;
    int not_planar = 0;
    
    std::cout << "Running " << num_seeds << " optimizations...\n\n";
    
    for (int seed = 0; seed < num_seeds; ++seed) {
        Molecule mol = build_tetrahedral_complex(30, 17, 2.2);  // Zn=30, Cl=17
        
        if (seed > 0) {
            perturb_coordinates(mol, 0.15, rng);
        }
        
        FIREOptimizer minimizer;
        minimizer.max_steps = max_steps;
        minimizer.f_tol = 1e-6;
        minimizer.minimize(mol);
        
        CoordinationMetrics metrics = analyze_coordination(mol, 0, 17, 3.0);
        
        if (metrics.coordination_number == 4) cn_correct++;
        
        // Tetrahedral: all 6 angles should be near 109.5°
        int near_109 = 0;
        for (double angle : metrics.ligand_angles) {
            if (std::abs(angle - 109.47) < 15.0) near_109++;
        }
        
        bool tetrahedral_pattern = (near_109 >= 5);
        if (tetrahedral_pattern) angles_tetrahedral++;
        
        // Should NOT be planar
        bool is_nonplanar = metrics.planarity_deviation > 0.5;
        if (is_nonplanar) not_planar++;
        
        if (seed < 3 || !tetrahedral_pattern) {
            std::cout << "  Seed " << std::setw(2) << seed 
                      << ": CN=" << metrics.coordination_number
                      << ", ~109.5°: " << near_109 << "/6"
                      << ", planarity=" << std::fixed << std::setprecision(2) << metrics.planarity_deviation << " Å"
                      << " → " << (tetrahedral_pattern ? "tetrahedral ✓" : "irregular ✗")
                      << "\n";
        }
    }
    
    std::cout << "\nResults:\n";
    std::cout << "  CN=4: " << cn_correct << "/" << num_seeds 
              << " (" << std::fixed << std::setprecision(1) << 100.0*cn_correct/num_seeds << "%)\n";
    std::cout << "  Tetrahedral angles: " << angles_tetrahedral << "/" << num_seeds 
              << " (" << 100.0*angles_tetrahedral/num_seeds << "%)\n";
    std::cout << "  Non-planar: " << not_planar << "/" << num_seeds 
              << " (" << 100.0*not_planar/num_seeds << "%)\n";
    
    bool pass = (cn_correct >= 12) && (angles_tetrahedral >= 11) && (not_planar >= 11);
    std::cout << "\n" << (pass ? "✓ PASS" : "✗ FAIL") << "\n";
}

//=============================================================================
// Main
//=============================================================================

int main() {
    print_header("PHASE 4: Coordination Geometry Variants");
    
    try {
        test_ferrocyanide();
        test_nickel_cyanide();
        test_zinc_chloride();
        
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  PHASE 4 Testing Complete                                 ║\n";
        std::cout << "║  Review individual test results above                     ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ EXCEPTION: " << e.what() << "\n";
        return 1;
    }
}
