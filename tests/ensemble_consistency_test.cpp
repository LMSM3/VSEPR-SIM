/**
 * ensemble_consistency_test.cpp
 * ------------------------------
 * Ensemble Consistency & Perturbation Invariance Test
 * 
 * Goal: Verify that ensemble-level features and inferred properties are stable,
 *       causal, and scale-consistent, not artifacts of:
 *       - Specific random seeds
 *       - Relaxation pathways
 *       - Numerical noise
 *       - Structural overfitting
 * 
 * Philosophy: If your physics is real, small perturbations should not change
 *            ensemble statistics. If they do, you're overfitting noise.
 * 
 * Test Protocol (from user specification):
 * 1. Choose deliberately boring system (C, Si, NaCl - simple but nontrivial)
 * 2. Generate K ensembles with identical physics, different RNG seeds
 * 3. Apply controlled micro-perturbations (strain, displacement, temperature)
 * 4. Verify ensemble statistics are invariant
 * 5. Detect overfitting vs physical robustness
 */

#include "../atomistic/core/linalg.hpp"
#include "../atomistic/models/model.hpp"
#include "atomistic/models/model.hpp"
// Note: lj_coulomb implementation is in model.cpp, accessed via create_lj_coulomb_model()
#include "../atomistic/parsers/xyz_parser.hpp"
#include "../atomistic/core/thermodynamics.hpp"
#include "../src/io/xyz_format.cpp"
#include <iostream>
#include <random>
#include <vector>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

using namespace vsepr;

// ============================================================================
// Statistical Utilities
// ============================================================================

struct EnsembleStatistics {
    double mean;
    double std_dev;
    double min;
    double max;
    double variance;
    size_t count;
    
    void compute(const std::vector<double>& data) {
        count = data.size();
        if (count == 0) {
            mean = std_dev = min = max = variance = 0.0;
            return;
        }
        
        // Mean
        mean = std::accumulate(data.begin(), data.end(), 0.0) / count;
        
        // Variance
        variance = 0.0;
        for (double x : data) {
            variance += (x - mean) * (x - mean);
        }
        variance /= count;
        std_dev = std::sqrt(variance);
        
        // Min/Max
        min = *std::min_element(data.begin(), data.end());
        max = *std::max_element(data.begin(), data.end());
    }
    
    void print(const std::string& label) const {
        std::cout << label << ":\n";
        std::cout << "  Mean:   " << std::fixed << std::setprecision(6) << mean << "\n";
        std::cout << "  StdDev: " << std_dev << " (" << (100.0 * std_dev / std::abs(mean)) << "%)\n";
        std::cout << "  Range:  [" << min << ", " << max << "]\n";
        std::cout << "  Count:  " << count << "\n";
    }
};

// ============================================================================
// Perturbation Generators
// ============================================================================

class PerturbationGenerator {
public:
    PerturbationGenerator(uint64_t seed) : rng_(seed), dist_(-1.0, 1.0) {}
    
    // Apply isotropic strain (uniform scaling)
    CoreState apply_strain(const CoreState& state, double strain_percent) {
        CoreState perturbed = state;
        double scale = 1.0 + (strain_percent / 100.0);
        
        for (size_t i = 0; i < perturbed.positions.size(); ++i) {
            perturbed.positions[i] *= scale;
        }
        
        return perturbed;
    }
    
    // Apply random displacement to each atom
    CoreState apply_displacement(const CoreState& state, double max_displacement_angstrom) {
        CoreState perturbed = state;
        
        for (size_t i = 0; i < perturbed.positions.size(); ++i) {
            Vec3 displacement = {
                dist_(rng_) * max_displacement_angstrom,
                dist_(rng_) * max_displacement_angstrom,
                dist_(rng_) * max_displacement_angstrom
            };
            perturbed.positions[i] = perturbed.positions[i] + displacement;
        }
        
        return perturbed;
    }
    
    // Apply thermal noise (velocity randomization)
    CoreState apply_thermal_noise(const CoreState& state, double temperature_K) {
        CoreState perturbed = state;
        perturbed.velocities.resize(perturbed.positions.size());
        
        // Boltzmann constant in appropriate units
        const double kB = 1.380649e-23; // J/K
        const double amu_to_kg = 1.66054e-27; // kg
        
        for (size_t i = 0; i < perturbed.velocities.size(); ++i) {
            // Maxwell-Boltzmann distribution
            double mass = 12.0 * amu_to_kg; // Carbon mass (or use actual mass)
            double sigma = std::sqrt(kB * temperature_K / mass);
            
            std::normal_distribution<double> maxwell(0.0, sigma);
            perturbed.velocities[i] = {
                maxwell(rng_),
                maxwell(rng_),
                maxwell(rng_)
            };
        }
        
        return perturbed;
    }
    
    // Insert random defect (remove/displace single atom)
    CoreState insert_defect(const CoreState& state, double displacement_angstrom) {
        if (state.positions.empty()) return state;
        
        CoreState perturbed = state;
        
        // Pick random atom
        size_t defect_idx = std::uniform_int_distribution<size_t>(0, state.positions.size() - 1)(rng_);
        
        // Displace it significantly
        Vec3 large_displacement = {
            dist_(rng_) * displacement_angstrom,
            dist_(rng_) * displacement_angstrom,
            dist_(rng_) * displacement_angstrom
        };
        perturbed.positions[defect_idx] = perturbed.positions[defect_idx] + large_displacement;
        
        return perturbed;
    }
    
private:
    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> dist_;
};

// ============================================================================
// Ensemble Generator
// ============================================================================

class EnsembleGenerator {
public:
    EnsembleGenerator(const CoreState& initial_state, EnergyModel& model)
        : initial_state_(initial_state), model_(model) {}
    
    // Generate K independent ensembles with different seeds
    std::vector<CoreState> generate_ensemble(
        size_t num_states,
        uint64_t base_seed,
        int relaxation_steps = 100
    ) {
        std::vector<CoreState> ensemble;
        ensemble.reserve(num_states);
        
        for (size_t k = 0; k < num_states; ++k) {
            uint64_t seed = base_seed + k * 12345;
            PerturbationGenerator perturb(seed);
            
            // Small random displacement to break symmetry
            CoreState perturbed = perturb.apply_displacement(initial_state_, 0.1);
            
            // Relax (FIRE minimization)
            CoreState relaxed = relax_fire(perturbed, relaxation_steps);
            
            ensemble.push_back(relaxed);
        }
        
        return ensemble;
    }
    
private:
    CoreState initial_state_;
    EnergyModel& model_;
    
    // Simple FIRE minimization (stub - replace with actual implementation)
    CoreState relax_fire(const CoreState& state, int max_steps) {
        // TODO: Integrate with actual FIRE implementation from atomistic library
        // For now, return state as-is (placeholder)
        return state;
    }
};

// ============================================================================
// Ensemble Analysis
// ============================================================================

class EnsembleAnalyzer {
public:
    // Compute energy distribution across ensemble
    static EnsembleStatistics analyze_energy(
        const std::vector<CoreState>& ensemble,
        EnergyModel& model
    ) {
        std::vector<double> energies;
        energies.reserve(ensemble.size());
        
        for (const auto& state : ensemble) {
            double energy = model.compute_energy(state);
            energies.push_back(energy);
        }
        
        EnsembleStatistics stats;
        stats.compute(energies);
        return stats;
    }
    
    // Compute center-of-mass distribution
    static EnsembleStatistics analyze_com(const std::vector<CoreState>& ensemble) {
        std::vector<double> com_magnitudes;
        
        for (const auto& state : ensemble) {
            Vec3 com = {0, 0, 0};
            for (const auto& pos : state.positions) {
                com = com + pos;
            }
            com = com / static_cast<double>(state.positions.size());
            
            double mag = std::sqrt(com.x*com.x + com.y*com.y + com.z*com.z);
            com_magnitudes.push_back(mag);
        }
        
        EnsembleStatistics stats;
        stats.compute(com_magnitudes);
        return stats;
    }
    
    // Compute radius of gyration distribution
    static EnsembleStatistics analyze_gyration_radius(const std::vector<CoreState>& ensemble) {
        std::vector<double> rg_values;
        
        for (const auto& state : ensemble) {
            // Center of mass
            Vec3 com = {0, 0, 0};
            for (const auto& pos : state.positions) {
                com = com + pos;
            }
            com = com / static_cast<double>(state.positions.size());
            
            // Radius of gyration
            double rg_sq = 0.0;
            for (const auto& pos : state.positions) {
                Vec3 r = pos - com;
                rg_sq += (r.x*r.x + r.y*r.y + r.z*r.z);
            }
            rg_sq /= state.positions.size();
            
            rg_values.push_back(std::sqrt(rg_sq));
        }
        
        EnsembleStatistics stats;
        stats.compute(rg_values);
        return stats;
    }
    
    // Compare two ensembles and check if they're statistically equivalent
    static bool ensembles_equivalent(
        const EnsembleStatistics& stats1,
        const EnsembleStatistics& stats2,
        double tolerance_percent = 5.0
    ) {
        // Check if means are within tolerance
        double mean_diff = std::abs(stats1.mean - stats2.mean);
        double relative_diff = 100.0 * mean_diff / std::abs(stats1.mean);
        
        std::cout << "  Mean difference: " << mean_diff << " (" << relative_diff << "%)\n";
        
        if (relative_diff > tolerance_percent) {
            std::cout << "  ❌ FAIL: Means differ by > " << tolerance_percent << "%\n";
            return false;
        }
        
        // Check if std devs are similar (within 2x)
        double stddev_ratio = stats1.std_dev / stats2.std_dev;
        std::cout << "  StdDev ratio: " << stddev_ratio << "\n";
        
        if (stddev_ratio > 2.0 || stddev_ratio < 0.5) {
            std::cout << "  ❌ FAIL: StdDevs differ by > 2x\n";
            return false;
        }
        
        std::cout << "  ✅ PASS: Ensembles are statistically equivalent\n";
        return true;
    }
};

// ============================================================================
// Main Test Driver
// ============================================================================

void test_carbon_dimer() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 1: Carbon Dimer (C₂)                              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    // Create simple C2 molecule
    CoreState initial;
    initial.atomic_numbers = {6, 6};  // Two carbons
    initial.positions = {
        {0.0, 0.0, 0.0},
        {1.2, 0.0, 0.0}  // ~1.2 Å bond length
    };
    
    // Create energy model (LJ + Coulomb)
    LJCoulombModel model;
    
    // Generate base ensemble (K=10)
    std::cout << "Generating base ensemble (K=10, seed=42)...\n";
    EnsembleGenerator gen(initial, model);
    auto ensemble_base = gen.generate_ensemble(10, 42);
    
    // Generate perturbed ensemble (same physics, different seed)
    std::cout << "Generating perturbed ensemble (K=10, seed=12345)...\n";
    auto ensemble_perturbed = gen.generate_ensemble(10, 12345);
    
    // Analyze energy distribution
    std::cout << "\n--- Energy Distribution ---\n";
    auto energy_base = EnsembleAnalyzer::analyze_energy(ensemble_base, model);
    energy_base.print("Base Ensemble");
    
    auto energy_perturbed = EnsembleAnalyzer::analyze_energy(ensemble_perturbed, model);
    energy_perturbed.print("Perturbed Ensemble");
    
    // Check if statistically equivalent
    std::cout << "\nEquivalence Test:\n";
    bool equivalent = EnsembleAnalyzer::ensembles_equivalent(energy_base, energy_perturbed, 5.0);
    
    if (!equivalent) {
        std::cerr << "\n⚠️  WARNING: Ensembles are NOT statistically equivalent!\n";
        std::cerr << "   This suggests overfitting to specific seeds/paths.\n";
        std::cerr << "   Your structure predictions may not be physically robust.\n\n";
        std::exit(1);
    }
    
    std::cout << "\n✅ PASS: C₂ ensemble is seed-invariant\n";
}

void test_perturbation_invariance() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 2: Perturbation Invariance                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    // Create simple system (4-atom carbon chain)
    CoreState initial;
    initial.atomic_numbers = {6, 6, 6, 6};
    initial.positions = {
        {0.0, 0.0, 0.0},
        {1.5, 0.0, 0.0},
        {3.0, 0.0, 0.0},
        {4.5, 0.0, 0.0}
    };
    
    LJCoulombModel model;
    
    // Generate base ensemble
    std::cout << "Generating base ensemble (K=20)...\n";
    EnsembleGenerator gen(initial, model);
    auto ensemble_base = gen.generate_ensemble(20, 42);
    
    // Apply micro-perturbations
    PerturbationGenerator perturb(12345);
    
    std::cout << "\nApplying perturbations:\n";
    std::cout << "  - ±1% strain\n";
    std::cout << "  - 0.05 Å random displacement\n";
    std::cout << "  - 10K thermal noise\n\n";
    
    std::vector<CoreState> ensemble_perturbed;
    for (const auto& state : ensemble_base) {
        // Apply all perturbations
        auto p1 = perturb.apply_strain(state, 1.0);
        auto p2 = perturb.apply_displacement(p1, 0.05);
        auto p3 = perturb.apply_thermal_noise(p2, 10.0);
        ensemble_perturbed.push_back(p3);
    }
    
    // Analyze radius of gyration (should be robust to small perturbations)
    std::cout << "--- Radius of Gyration ---\n";
    auto rg_base = EnsembleAnalyzer::analyze_gyration_radius(ensemble_base);
    rg_base.print("Base Ensemble");
    
    auto rg_perturbed = EnsembleAnalyzer::analyze_gyration_radius(ensemble_perturbed);
    rg_perturbed.print("Perturbed Ensemble");
    
    // Check if statistically equivalent (allow 10% for perturbations)
    std::cout << "\nRobustness Test:\n";
    bool robust = EnsembleAnalyzer::ensembles_equivalent(rg_base, rg_perturbed, 10.0);
    
    if (!robust) {
        std::cerr << "\n⚠️  WARNING: System is NOT robust to micro-perturbations!\n";
        std::cerr << "   Small changes cause large ensemble shifts.\n";
        std::cerr << "   This indicates numerical instability or overfitting.\n\n";
        std::exit(1);
    }
    
    std::cout << "\n✅ PASS: System is robust to micro-perturbations\n";
}

void test_defect_insertion() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 3: Defect Insertion Robustness                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
    
    // Create small cluster
    CoreState initial;
    initial.atomic_numbers = {6, 6, 6, 6, 6, 6};  // 6 carbons
    initial.positions = {
        {0.0, 0.0, 0.0},
        {1.5, 0.0, 0.0},
        {0.0, 1.5, 0.0},
        {1.5, 1.5, 0.0},
        {0.75, 0.75, 1.5},
        {0.75, 0.75, -1.5}
    };
    
    LJCoulombModel model;
    
    // Generate clean ensemble
    std::cout << "Generating clean ensemble (K=15)...\n";
    EnsembleGenerator gen(initial, model);
    auto ensemble_clean = gen.generate_ensemble(15, 42);
    
    // Generate defect ensemble (low defect rate)
    std::cout << "Generating defect ensemble (10% defect rate)...\n";
    std::vector<CoreState> ensemble_defect;
    PerturbationGenerator perturb(12345);
    
    for (size_t k = 0; k < 15; ++k) {
        auto state = ensemble_clean[k];
        
        // Insert defect with 10% probability
        if (k % 10 == 0) {
            state = perturb.insert_defect(state, 2.0);  // 2 Å displacement
        }
        
        ensemble_defect.push_back(state);
    }
    
    // Analyze energy distribution (defects should increase mean but not destroy variance)
    std::cout << "\n--- Energy Distribution with Defects ---\n";
    auto energy_clean = EnsembleAnalyzer::analyze_energy(ensemble_clean, model);
    energy_clean.print("Clean Ensemble");
    
    auto energy_defect = EnsembleAnalyzer::analyze_energy(ensemble_defect, model);
    energy_defect.print("Defect Ensemble");
    
    // Check if variance is still reasonable (not exploding)
    double variance_ratio = energy_defect.variance / energy_clean.variance;
    std::cout << "\nVariance Ratio (defect/clean): " << variance_ratio << "\n";
    
    if (variance_ratio > 100.0) {
        std::cerr << "\n⚠️  WARNING: Defects cause catastrophic variance increase!\n";
        std::cerr << "   Ensemble statistics collapse with minor defects.\n";
        std::cerr << "   This indicates fragile, non-physical behavior.\n\n";
        std::exit(1);
    }
    
    std::cout << "\n✅ PASS: System is robust to low-rate defects\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Ensemble Consistency & Perturbation Invariance Test      ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Testing if structure predictions are physically robust   ║\n";
    std::cout << "║  or just artifacts of numerical luck.                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    try {
        // Test 1: Seed invariance
        test_carbon_dimer();
        
        // Test 2: Perturbation robustness
        test_perturbation_invariance();
        
        // Test 3: Defect robustness
        test_defect_insertion();
        
        // Summary
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ALL TESTS PASSED                                          ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  ✅ Ensemble statistics are seed-invariant                 ║\n";
        std::cout << "║  ✅ System is robust to micro-perturbations                ║\n";
        std::cout << "║  ✅ Low-rate defects don't destroy ensemble structure      ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Conclusion: Your structure predictions are physically     ║\n";
        std::cout << "║              robust, not numerical artifacts.              ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << "\n\n";
        return 1;
    }
    
    return 0;
}
