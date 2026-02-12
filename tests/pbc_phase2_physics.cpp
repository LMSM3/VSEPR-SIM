/**
 * pbc_phase2_physics.cpp - Phase 2: Pairwise Physics Parity
 * 
 * Verifies PBC is used consistently in physics calculations:
 * - Newton's 3rd law with MIC (forces balanced)
 * - System translation invariance (energy/forces unchanged)
 * - Edge stress cloud (boundary handling)
 * 
 * Uses simple O(N²) Lennard-Jones without neighbor lists.
 */

#include "box/pbc.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

using namespace vsepr;

// Tolerances for physics checks
const double TOL_ENERGY = 1e-6;      // Energy comparison tolerance (relaxed for numerical precision)
const double TOL_FORCE = 1e-6;       // Force comparison tolerance (relaxed)
const double EPS = 1e-12;            // Geometric tolerance

// Simple Lennard-Jones parameters
struct LJParams {
    double sigma = 3.0;    // Collision diameter (Å)
    double epsilon = 0.1;  // Well depth (kcal/mol)
    double cutoff = 9.0;   // Cutoff distance (Å)
};

// Particle system
struct System {
    std::vector<Vec3> positions;
    std::vector<Vec3> forces;
    double energy = 0.0;
    
    System(int n = 0) : positions(n), forces(n) {}
    
    void resize(int n) {
        positions.resize(n);
        forces.resize(n);
    }
    
    void zero_forces() {
        for (auto& f : forces) {
            f = Vec3(0, 0, 0);
        }
        energy = 0.0;
    }
};

/**
 * Compute Lennard-Jones energy and forces with PBC
 * Simple O(N²) implementation - no neighbor lists
 */
void compute_lj_pbc(System& sys, const BoxOrtho& box, const LJParams& params) {
    sys.zero_forces();
    
    const int N = sys.positions.size();
    const double sig6 = std::pow(params.sigma, 6);
    const double sig12 = sig6 * sig6;
    const double cutoff2 = params.cutoff * params.cutoff;
    
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            // Minimum image displacement
            Vec3 dr = box.delta(sys.positions[i], sys.positions[j]);
            double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
            
            if (r2 < cutoff2 && r2 > 1e-10) {
                double r2inv = 1.0 / r2;
                double r6inv = r2inv * r2inv * r2inv;
                double r12inv = r6inv * r6inv;
                
                // Energy: 4*eps*[(sig/r)^12 - (sig/r)^6]
                double e_lj = 4.0 * params.epsilon * (sig12 * r12inv - sig6 * r6inv);
                sys.energy += e_lj;
                
                // Force magnitude: 24*eps*[2*(sig/r)^12 - (sig/r)^6] / r
                double f_mag = 24.0 * params.epsilon * (2.0 * sig12 * r12inv - sig6 * r6inv) * r2inv;
                
                // Force vector (on i from j)
                Vec3 f = dr * f_mag;
                
                // Newton's 3rd law
                sys.forces[i] = sys.forces[i] + f;
                sys.forces[j] = sys.forces[j] - f;
            }
        }
    }
}

/**
 * Compute force on particle i from particle j (single pair)
 */
Vec3 compute_pair_force(const Vec3& ri, const Vec3& rj, const BoxOrtho& box, const LJParams& params) {
    Vec3 dr = box.delta(ri, rj);
    double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
    
    const double sig6 = std::pow(params.sigma, 6);
    const double sig12 = sig6 * sig6;
    const double cutoff2 = params.cutoff * params.cutoff;
    
    if (r2 < cutoff2 && r2 > 1e-10) {
        double r2inv = 1.0 / r2;
        double r6inv = r2inv * r2inv * r2inv;
        double r12inv = r6inv * r6inv;
        
        double f_mag = 24.0 * params.epsilon * (2.0 * sig12 * r12inv - sig6 * r6inv) * r2inv;
        return dr * f_mag;
    }
    
    return Vec3(0, 0, 0);
}

// ============================================================================
// Test 3: Newton's 3rd Law (Pair-Level)
// ============================================================================

void test_newtons_third_law(int& passed, int& failed) {
    std::cout << "\n=== Test 3: Newton's 3rd Law (Pair-Level) ===\n";
    
    BoxOrtho box(20.0, 20.0, 20.0);
    LJParams params;
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 20.0);
    
    const int N_TESTS = 100;
    int violations = 0;
    
    for (int test = 0; test < N_TESTS; ++test) {
        // Random pair of particles
        Vec3 ri(dist(rng), dist(rng), dist(rng));
        Vec3 rj(dist(rng), dist(rng), dist(rng));
        
        // Compute forces both ways
        Vec3 f_ij = compute_pair_force(ri, rj, box, params);  // Force on i from j
        Vec3 f_ji = compute_pair_force(rj, ri, box, params);  // Force on j from i
        
        // Check Newton's 3rd law: F_ij = -F_ji
        Vec3 sum = f_ij + f_ji;
        double violation = sum.norm();
        
        if (violation > TOL_FORCE) {
            violations++;
            if (violations <= 3) {  // Print first few violations
                std::cout << "  ✗ Violation #" << violations << ":\n";
                std::cout << "    ri: (" << ri.x << ", " << ri.y << ", " << ri.z << ")\n";
                std::cout << "    rj: (" << rj.x << ", " << rj.y << ", " << rj.z << ")\n";
                std::cout << "    F_ij: (" << f_ij.x << ", " << f_ij.y << ", " << f_ij.z << ")\n";
                std::cout << "    F_ji: (" << f_ji.x << ", " << f_ji.y << ", " << f_ji.z << ")\n";
                std::cout << "    ||F_ij + F_ji||: " << violation << "\n";
            }
        }
    }
    
    if (violations == 0) {
        std::cout << "  ✓ Newton's 3rd law verified for " << N_TESTS << " random pairs\n";
        std::cout << "    ||F_ij + F_ji|| < " << TOL_FORCE << " for all pairs\n";
        passed++;
    } else {
        std::cout << "  ✗ FAILED: " << violations << "/" << N_TESTS << " pairs violated Newton's 3rd law\n";
        failed++;
    }
}

// ============================================================================
// Test 4: System Translation Invariance
// ============================================================================

void test_translation_invariance(int& passed, int& failed) {
    std::cout << "\n=== Test 4: System Translation Invariance ===\n";
    
    BoxOrtho box(15.0, 15.0, 15.0);
    LJParams params;
    
    // Create a small system with random particles
    const int N = 10;
    System sys(N);
    
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(2.0, 13.0);
    
    for (int i = 0; i < N; ++i) {
        sys.positions[i] = Vec3(dist(rng), dist(rng), dist(rng));
    }
    
    // Compute original energy and forces
    compute_lj_pbc(sys, box, params);
    double E0 = sys.energy;
    std::vector<Vec3> F0 = sys.forces;
    
    std::cout << "  Original system:\n";
    std::cout << "    Energy: " << E0 << " kcal/mol\n";
    
    // Test multiple translations
    std::uniform_int_distribution<int> int_dist(-3, 3);
    const int N_TRANSLATIONS = 20;
    int energy_violations = 0;
    int force_violations = 0;
    
    for (int test = 0; test < N_TRANSLATIONS; ++test) {
        // Random integer cell shift
        Vec3 shift(int_dist(rng) * box.L.x, 
                   int_dist(rng) * box.L.y, 
                   int_dist(rng) * box.L.z);
        
        // Shift all particles (without wrapping initially)
        System sys_shifted(N);
        for (int i = 0; i < N; ++i) {
            sys_shifted.positions[i] = sys.positions[i] + shift;
        }
        
        // Compute energy and forces
        compute_lj_pbc(sys_shifted, box, params);
        
        // Check energy invariance (use relative tolerance for large energies)
        double dE = std::abs(sys_shifted.energy - E0);
        double rel_err = (E0 != 0.0) ? dE / std::abs(E0) : dE;
        
        if (rel_err > 1e-10) {  // Relative tolerance: 10^-10
            energy_violations++;
            if (energy_violations <= 2) {
                std::cout << "  ✗ Energy violation #" << energy_violations << ":\n";
                std::cout << "    Shift: (" << shift.x/box.L.x << "Lx, " 
                          << shift.y/box.L.y << "Ly, " << shift.z/box.L.z << "Lz)\n";
                std::cout << "    E0: " << E0 << ", E': " << sys_shifted.energy << "\n";
                std::cout << "    |E' - E0|: " << dE << " (relative: " << rel_err << ")\n";
            }
        }
        
        // Check force invariance (use relative tolerance for large forces)
        double max_force_diff = 0.0;
        double max_force_rel = 0.0;
        for (int i = 0; i < N; ++i) {
            Vec3 df = sys_shifted.forces[i] - F0[i];
            double diff = df.norm();
            double f_mag = F0[i].norm();
            double rel = (f_mag > 1e-10) ? diff / f_mag : diff;
            
            max_force_diff = std::max(max_force_diff, diff);
            max_force_rel = std::max(max_force_rel, rel);
        }
        
        if (max_force_rel > 1e-10) {  // Relative tolerance: 10^-10
            force_violations++;
            if (force_violations <= 2) {
                std::cout << "  ✗ Force violation #" << force_violations << ":\n";
                std::cout << "    Shift: (" << shift.x/box.L.x << "Lx, " 
                          << shift.y/box.L.y << "Ly, " << shift.z/box.L.z << "Lz)\n";
                std::cout << "    Max force diff: " << max_force_diff 
                          << " (relative: " << max_force_rel << ")\n";
            }
        }
    }
    
    std::cout << "  Tested " << N_TRANSLATIONS << " random integer cell translations\n";
    
    bool energy_ok = (energy_violations == 0);
    bool forces_ok = (force_violations == 0);
    
    if (energy_ok) {
        std::cout << "  ✓ Energy invariance: relative error < 1e-10 for all shifts\n";
    } else {
        std::cout << "  ✗ Energy violations: " << energy_violations << "/" << N_TRANSLATIONS << "\n";
    }
    
    if (forces_ok) {
        std::cout << "  ✓ Force invariance: relative error < 1e-10 for all shifts\n";
    } else {
        std::cout << "  ✗ Force violations: " << force_violations << "/" << N_TRANSLATIONS << "\n";
    }
    
    if (energy_ok && forces_ok) {
        passed++;
    } else {
        failed++;
    }
}

// ============================================================================
// Test 5: Edge Stress Cloud
// ============================================================================

void test_edge_stress_cloud(int& passed, int& failed) {
    std::cout << "\n=== Test 5: Edge Stress Cloud (Boundary Handling) ===\n";
    
    // Use larger box to fit more particles comfortably
    BoxOrtho box(15.0, 15.0, 15.0);
    LJParams params;
    
    // Place particles near boundaries: [0, 0.2L] ∪ [0.8L, L) with minimum separation
    const int N = 12;  // Fewer particles to ensure placement success
    const double r_min = 2.5;  // Minimum separation (Angstroms)
    System sys(N);
    
    std::mt19937 rng(456);
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    
    std::cout << "  Placing " << N << " particles near boundaries with r_min = " << r_min << " Å...\n";
    
    const int max_attempts = 1000;
    for (int i = 0; i < N; ++i) {
        bool placed = false;
        
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            Vec3 candidate;
            
            for (int dim = 0; dim < 3; ++dim) {
                double L = (dim == 0) ? box.L.x : ((dim == 1) ? box.L.y : box.L.z);
                
                // Randomly choose low edge [0, 0.2L] or high edge [0.8L, L)
                double pos;
                if (coin(rng) < 0.5) {
                    pos = coin(rng) * 0.2 * L;  // Low edge
                } else {
                    pos = 0.8 * L + coin(rng) * 0.2 * L;  // High edge
                }
                
                if (dim == 0) candidate.x = pos;
                else if (dim == 1) candidate.y = pos;
                else candidate.z = pos;
            }
            
            // Check minimum separation from all previously placed particles
            bool too_close = false;
            for (int j = 0; j < i; ++j) {
                Vec3 dr = box.delta(sys.positions[j], candidate);
                double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
                if (r2 < r_min * r_min) {
                    too_close = true;
                    break;
                }
            }
            
            if (!too_close) {
                sys.positions[i] = candidate;
                placed = true;
                break;
            }
        }
        
        if (!placed) {
            std::cout << "  ✗ Failed to place particle " << i << " with minimum separation\n";
            std::cout << "  ✗ FAILED: Configuration generation failed\n";
            failed++;
            return;
        }
    }
    
    std::cout << "  System: " << N << " particles near boundaries\n";
    std::cout << "  Regions: [0, 3Å] ∪ [12Å, 15Å] in each dimension\n";
    
    // Compute initial energy
    compute_lj_pbc(sys, box, params);
    double E0 = sys.energy;
    
    std::cout << "  Initial energy: " << E0 << " kcal/mol\n";
    
    // Check for finite forces
    bool forces_finite = true;
    double max_force = 0.0;
    for (int i = 0; i < N; ++i) {
        double f_mag = sys.forces[i].norm();
        if (!std::isfinite(f_mag)) {
            forces_finite = false;
            std::cout << "  ✗ Non-finite force on particle " << i << "\n";
        }
        max_force = std::max(max_force, f_mag);
    }
    
    if (!forces_finite) {
        std::cout << "  ✗ FAILED: Non-finite forces detected\n";
        failed++;
        return;
    }
    
    std::cout << "  ✓ All forces finite (max: " << max_force << " kcal/mol/Å)\n";
    
    // Apply small perturbations and check for energy continuity
    const int N_PERTURBATIONS = 10;
    const double perturbation = 0.01;  // Small displacement (Å)
    
    std::vector<double> energies;
    energies.push_back(E0);
    
    for (int pert = 0; pert < N_PERTURBATIONS; ++pert) {
        // Perturb random particle by small amount
        int i = rng() % N;
        Vec3 delta(perturbation * (2.0*coin(rng) - 1.0),
                   perturbation * (2.0*coin(rng) - 1.0),
                   perturbation * (2.0*coin(rng) - 1.0));
        
        sys.positions[i] = sys.positions[i] + delta;
        sys.positions[i] = box.wrap(sys.positions[i]);  // Wrap if crossed boundary
        
        compute_lj_pbc(sys, box, params);
        energies.push_back(sys.energy);
    }
    
    // Check for energy spikes (discontinuities)
    bool no_spikes = true;
    for (size_t i = 1; i < energies.size(); ++i) {
        double dE = std::abs(energies[i] - energies[i-1]);
        
        // Energy change should be small for small perturbations
        // Allow up to ~10 kcal/mol change (reasonable for LJ with these parameters)
        if (dE > 10.0) {
            std::cout << "  ✗ Energy spike detected: " << energies[i-1] 
                      << " → " << energies[i] << " (ΔE = " << dE << ")\n";
            no_spikes = false;
        }
    }
    
    if (no_spikes) {
        std::cout << "  ✓ No energy discontinuities over " << N_PERTURBATIONS 
                  << " small perturbations\n";
        std::cout << "    Energy range: [" << *std::min_element(energies.begin(), energies.end())
                  << ", " << *std::max_element(energies.begin(), energies.end()) << "] kcal/mol\n";
        passed++;
    } else {
        std::cout << "  ✗ FAILED: Energy discontinuities detected\n";
        failed++;
    }
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << std::fixed << std::setprecision(10);
    
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PBC Phase 2 — Pairwise Physics Parity Tests             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\nLennard-Jones Parameters:\n";
    std::cout << "  σ = 3.0 Å (collision diameter)\n";
    std::cout << "  ε = 0.1 kcal/mol (well depth)\n";
    std::cout << "  r_cut = 9.0 Å (cutoff distance)\n";
    std::cout << "\nTolerances:\n";
    std::cout << "  Energy: " << TOL_ENERGY << "\n";
    std::cout << "  Force: " << TOL_FORCE << "\n";
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PHASE 2 PHYSICS TESTS\n";
    std::cout << std::string(60, '=') << "\n";
    
    test_newtons_third_law(passed, failed);
    test_translation_invariance(passed, failed);
    test_edge_stress_cloud(passed, failed);
    
    // Final Verdict
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PHASE 2 FINAL VERDICT\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << "Tests Passed: " << passed << "\n";
    std::cout << "Tests Failed: " << failed << "\n";
    
    if (failed == 0) {
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✓✓✓ PHASE 2 COMPLETE — PHYSICS PARITY VERIFIED      ✓✓✓ ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\nPBC is used consistently in physics calculations.\n";
        std::cout << "Ready for production MD simulations.\n\n";
        return 0;
    } else {
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✗✗✗ PHASE 2 FAILED — PHYSICS INCONSISTENCY          ✗✗✗ ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n";
        std::cout << "\nFix physics integration before using PBC in production.\n\n";
        return 1;
    }
}
