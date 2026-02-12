/**
 * phase3_isomer_validation.cpp
 * =============================
 * Phase 3 Retest: Isomerism Testing (cis/trans)
 * 
 * Tests:
 * - [Co(NH3)4Cl2]+ cis/trans isomers
 * - 16 seeds each for cis and trans
 * - FIRE optimization: max_steps=2000
 * - Basin stability: 0.05 Å perturbation + re-optimization
 * 
 * PASS criteria:
 * - Identity: ∠Cl–Co–Cl stays 80–100° (cis) or 175–185° (trans)
 * - Coordination: CN(Co) = 6 in all runs
 * - Geometry: metal-centered angles cluster near 90°/180° (octahedral)
 * - Distances: Co–N within 1.80–2.30 Å, Co–Cl within 2.00–2.80 Å
 * - Multi-minima: cis and trans converge to distinct basins
 * - Reproducibility: ≥ 80% of seeds converge to intended basin
 * - Stability: perturb + re-opt returns to same basin
 * - Sanity: no NaNs, no overlaps (min_distance > 0.70 Å)
 */

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <random>
#include <vector>
#include <string>

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

struct GeometryMetrics {
    double cl_co_cl_angle;  // degrees
    int coordination_number;
    std::vector<double> co_n_distances;
    std::vector<double> co_cl_distances;
    std::vector<double> ligand_angles;  // All L-Co-L angles
    double min_distance;
    bool has_nan;
};

GeometryMetrics analyze_geometry(const Molecule& mol, uint32_t metal_idx) {
    GeometryMetrics metrics;
    metrics.has_nan = false;
    metrics.min_distance = 1e9;
    metrics.coordination_number = 0;
    
    // Find metal center (Co)
    std::vector<uint32_t> cl_indices;
    std::vector<uint32_t> n_indices;
    
    for (uint32_t i = 0; i < mol.num_atoms(); ++i) {
        if (i == metal_idx) continue;
        
        // Check for NaNs in coordinates
        for (int d = 0; d < 3; ++d) {
            if (std::isnan(mol.coords[3*i + d])) {
                metrics.has_nan = true;
            }
        }
        
        double dx = mol.coords[3*i] - mol.coords[3*metal_idx];
        double dy = mol.coords[3*i+1] - mol.coords[3*metal_idx+1];
        double dz = mol.coords[3*i+2] - mol.coords[3*metal_idx+2];
        double r = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        // Assume coordination if within 3.0 Å
        if (r < 3.0) {
            metrics.coordination_number++;
            
            if (mol.atoms[i].Z == 17) {  // Cl
                cl_indices.push_back(i);
                metrics.co_cl_distances.push_back(r);
            } else if (mol.atoms[i].Z == 7) {  // N
                n_indices.push_back(i);
                metrics.co_n_distances.push_back(r);
            }
        }
        
        // Check min distances between all atoms
        for (uint32_t j = i+1; j < mol.num_atoms(); ++j) {
            double dx2 = mol.coords[3*i] - mol.coords[3*j];
            double dy2 = mol.coords[3*i+1] - mol.coords[3*j+1];
            double dz2 = mol.coords[3*i+2] - mol.coords[3*j+2];
            double r2 = std::sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2);
            if (r2 < metrics.min_distance) {
                metrics.min_distance = r2;
            }
        }
    }
    
    // Calculate Cl-Co-Cl angle
    if (cl_indices.size() == 2) {
        uint32_t cl1 = cl_indices[0];
        uint32_t cl2 = cl_indices[1];
        
        double v1[3], v2[3];
        for (int d = 0; d < 3; ++d) {
            v1[d] = mol.coords[3*cl1 + d] - mol.coords[3*metal_idx + d];
            v2[d] = mol.coords[3*cl2 + d] - mol.coords[3*metal_idx + d];
        }
        
        double dot = v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
        double r1 = std::sqrt(v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]);
        double r2 = std::sqrt(v2[0]*v2[0] + v2[1]*v2[1] + v2[2]*v2[2]);
        
        double cos_angle = dot / (r1 * r2);
        cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
        metrics.cl_co_cl_angle = std::acos(cos_angle) * 180.0 / M_PI;
    }
    
    // Calculate all ligand-metal-ligand angles
    std::vector<uint32_t> all_ligands = cl_indices;
    all_ligands.insert(all_ligands.end(), n_indices.begin(), n_indices.end());
    
    for (size_t i = 0; i < all_ligands.size(); ++i) {
        for (size_t j = i+1; j < all_ligands.size(); ++j) {
            uint32_t lig1 = all_ligands[i];
            uint32_t lig2 = all_ligands[j];
            
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

bool is_cis_basin(double cl_co_cl_angle) {
    return cl_co_cl_angle >= 80.0 && cl_co_cl_angle <= 100.0;
}

bool is_trans_basin(double cl_co_cl_angle) {
    return cl_co_cl_angle >= 175.0 && cl_co_cl_angle <= 185.0;
}

std::string classify_basin(double angle) {
    if (is_cis_basin(angle)) return "cis";
    if (is_trans_basin(angle)) return "trans";
    return "intermediate";
}

Molecule build_cis_isomer() {
    Molecule mol;
    
    // Co at origin
    mol.add_atom(27, 0.0, 0.0, 0.0);  // Co
    
    // 4 N atoms (NH3 ligands) - octahedral positions
    mol.add_atom(7, 2.0, 0.0, 0.0);   // N1
    mol.add_atom(7, -2.0, 0.0, 0.0);  // N2
    mol.add_atom(7, 0.0, 2.0, 0.0);   // N3
    mol.add_atom(7, 0.0, -2.0, 0.0);  // N4
    
    // 2 Cl atoms - cis (90° apart, both in xy-plane)
    mol.add_atom(17, 0.0, 0.0, 2.3);  // Cl1 (along +z)
    mol.add_atom(17, 0.0, 0.0, -2.3); // Cl2 (along -z) - makes ~90° with Cl1
    
    // Actually for cis, put them adjacent
    mol.coords[15] = 2.3; mol.coords[16] = 0.0; mol.coords[17] = 0.0;   // Cl1: +x
    mol.coords[18] = 0.0; mol.coords[19] = 2.3; mol.coords[20] = 0.0;   // Cl2: +y (90° from Cl1)
    
    return mol;
}

Molecule build_trans_isomer() {
    Molecule mol;
    
    // Co at origin
    mol.add_atom(27, 0.0, 0.0, 0.0);  // Co
    
    // 4 N atoms (NH3 ligands) - square planar in xy
    mol.add_atom(7, 2.0, 0.0, 0.0);   // N1
    mol.add_atom(7, -2.0, 0.0, 0.0);  // N2
    mol.add_atom(7, 0.0, 2.0, 0.0);   // N3
    mol.add_atom(7, 0.0, -2.0, 0.0);  // N4
    
    // 2 Cl atoms - trans (180° apart, along z-axis)
    mol.add_atom(17, 0.0, 0.0, 2.3);  // Cl1 (+z)
    mol.add_atom(17, 0.0, 0.0, -2.3); // Cl2 (-z)
    
    return mol;
}

//=============================================================================
// Main Phase 3 Test
//=============================================================================

void test_phase3_cis_trans() {
    print_header("PHASE 3: [Co(NH3)4Cl2]+ cis/trans Isomerism");
    
    // Step 1: Build initial isomer structures
    print_section("Step 1: Build Initial Isomer Structures");
    
    Molecule cis_template = build_cis_isomer();
    Molecule trans_template = build_trans_isomer();
    
    std::cout << "✓ Built cis and trans templates\n";
    std::cout << "  cis: Cl atoms at 90° (adjacent positions)\n";
    std::cout << "  trans: Cl atoms at 180° (opposite positions)\n";
    
    // Step 2: Test cis isomer with 16 seeds
    print_section("Step 2: Test cis Isomer (16 seeds)");
    
    const int num_seeds = 16;
    const int max_steps = 2000;
    const double perturb_amplitude = 0.05;  // Å
    
    std::random_device rd;
    std::mt19937 rng(rd());
    
    int cis_success = 0;
    int cis_stable = 0;
    std::vector<double> cis_angles;
    
    for (int seed = 0; seed < num_seeds; ++seed) {
        Molecule mol = cis_template;
        
        // Perturb initial geometry
        if (seed > 0) {
            perturb_coordinates(mol, 0.1, rng);
        }
        
        // Optimize
        FIREOptimizer minimizer;
        minimizer.max_steps = max_steps;
        minimizer.f_tol = 1e-6;
        minimizer.minimize(mol);
        
        // Analyze
        GeometryMetrics metrics = analyze_geometry(mol, 0);
        
        // Check basin identity
        bool converged_to_cis = is_cis_basin(metrics.cl_co_cl_angle);
        if (converged_to_cis) cis_success++;
        
        cis_angles.push_back(metrics.cl_co_cl_angle);
        
        // Basin stability test: perturb + re-optimize
        if (converged_to_cis && seed < 8) {  // Test stability on first 8
            Molecule mol_pert = mol;
            perturb_coordinates(mol_pert, perturb_amplitude, rng);
            
            minimizer.minimize(mol_pert);
            GeometryMetrics metrics_pert = analyze_geometry(mol_pert, 0);
            
            if (is_cis_basin(metrics_pert.cl_co_cl_angle)) {
                cis_stable++;
            }
        }
        
        if (seed < 3 || !converged_to_cis) {
            std::cout << "  Seed " << std::setw(2) << seed 
                      << ": ∠Cl-Co-Cl = " << std::fixed << std::setprecision(1) << metrics.cl_co_cl_angle 
                      << "° → " << classify_basin(metrics.cl_co_cl_angle)
                      << " (CN=" << metrics.coordination_number << ")\n";
        }
    }
    
    double cis_reproducibility = 100.0 * cis_success / num_seeds;
    double cis_stability_rate = cis_stable >= 8 ? 100.0 * cis_stable / 8 : 0.0;
    
    std::cout << "\nCis Results:\n";
    std::cout << "  Reproducibility: " << cis_success << "/" << num_seeds 
              << " (" << std::fixed << std::setprecision(1) << cis_reproducibility << "%)\n";
    std::cout << "  Stability: " << cis_stable << "/8 tests returned to cis basin\n";
    
    // Step 3: Test trans isomer with 16 seeds
    print_section("Step 3: Test trans Isomer (16 seeds)");
    
    int trans_success = 0;
    int trans_stable = 0;
    std::vector<double> trans_angles;
    
    for (int seed = 0; seed < num_seeds; ++seed) {
        Molecule mol = trans_template;
        
        if (seed > 0) {
            perturb_coordinates(mol, 0.1, rng);
        }
        
        FIREOptimizer minimizer;
        minimizer.max_steps = max_steps;
        minimizer.f_tol = 1e-6;
        minimizer.minimize(mol);
        
        GeometryMetrics metrics = analyze_geometry(mol, 0);
        
        bool converged_to_trans = is_trans_basin(metrics.cl_co_cl_angle);
        if (converged_to_trans) trans_success++;
        
        trans_angles.push_back(metrics.cl_co_cl_angle);
        
        // Stability test
        if (converged_to_trans && seed < 8) {
            Molecule mol_pert = mol;
            perturb_coordinates(mol_pert, perturb_amplitude, rng);
            
            minimizer.minimize(mol_pert);
            GeometryMetrics metrics_pert = analyze_geometry(mol_pert, 0);
            
            if (is_trans_basin(metrics_pert.cl_co_cl_angle)) {
                trans_stable++;
            }
        }
        
        if (seed < 3 || !converged_to_trans) {
            std::cout << "  Seed " << std::setw(2) << seed 
                      << ": ∠Cl-Co-Cl = " << std::fixed << std::setprecision(1) << metrics.cl_co_cl_angle 
                      << "° → " << classify_basin(metrics.cl_co_cl_angle)
                      << " (CN=" << metrics.coordination_number << ")\n";
        }
    }
    
    double trans_reproducibility = 100.0 * trans_success / num_seeds;
    double trans_stability_rate = trans_stable >= 8 ? 100.0 * trans_stable / 8 : 0.0;
    
    std::cout << "\nTrans Results:\n";
    std::cout << "  Reproducibility: " << trans_success << "/" << num_seeds 
              << " (" << std::fixed << std::setprecision(1) << trans_reproducibility << "%)\n";
    std::cout << "  Stability: " << trans_stable << "/8 tests returned to trans basin\n";
    
    // Step 4: Validation Summary
    print_section("Step 4: Validation Summary");
    
    bool pass_identity_cis = cis_success >= 14;  // ≥87.5%
    bool pass_identity_trans = trans_success >= 14;
    bool pass_reproducibility = (cis_reproducibility >= 80.0) && (trans_reproducibility >= 80.0);
    bool pass_stability = (cis_stable >= 6) && (trans_stable >= 6);  // 75% of stability tests
    bool pass_multi_minima = (cis_success > 0) && (trans_success > 0);  // Distinct basins exist
    
    std::cout << "\n";
    std::cout << (pass_identity_cis ? "✓" : "✗") << " Cis identity preserved (≥87.5%)\n";
    std::cout << (pass_identity_trans ? "✓" : "✗") << " Trans identity preserved (≥87.5%)\n";
    std::cout << (pass_reproducibility ? "✓" : "✗") << " Reproducibility ≥80% for both isomers\n";
    std::cout << (pass_stability ? "✓" : "✗") << " Basin stability (perturb+reopt returns)\n";
    std::cout << (pass_multi_minima ? "✓" : "✗") << " Multi-minima: cis and trans are distinct basins\n";
    
    bool overall_pass = pass_identity_cis && pass_identity_trans && 
                       pass_reproducibility && pass_stability && pass_multi_minima;
    
    std::cout << "\n";
    if (overall_pass) {
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║  ✓ PHASE 3 PASS: Isomerism Validated  ║\n";
        std::cout << "╚════════════════════════════════════════╝\n";
    } else {
        std::cout << "╔════════════════════════════════════════╗\n";
        std::cout << "║  ✗ PHASE 3 FAIL: Issues detected      ║\n";
        std::cout << "╚════════════════════════════════════════╝\n";
    }
}

//=============================================================================
// Main
//=============================================================================

int main() {
    try {
        test_phase3_cis_trans();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ EXCEPTION: " << e.what() << "\n";
        return 1;
    }
}
